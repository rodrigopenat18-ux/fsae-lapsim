#include "app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>

#include <imgui.h>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>

#include "fsae/io/car_json.hpp"
#include "fsae/io/results.hpp"
#include "platform.hpp"

namespace fs = std::filesystem;

namespace fsae::gui {

namespace {

constexpr int kMaxRuns = 8;

/// Paleta categórica (instância de referência, passos para fundo escuro), em ordem fixa.
/// A cor segue a execução: uma execução nunca troca de cor quando outra é removida.
constexpr std::array<std::uint32_t, kMaxRuns> kRunColors = {
    0x3987e5, 0xd95926, 0x199e70, 0xc98500, 0xd55181, 0x008300, 0x9085e9, 0xe66767};

ImVec4 hex(std::uint32_t c, float a = 1.0f) {
    auto ch = [c](int shift) { return static_cast<float>(static_cast<int>((c >> shift) & 0xffu)) / 255.0f; };
    return ImVec4(ch(16), ch(8), ch(0), a);
}

const char* const kEvents[] = {"Autocross", "Skidpad", "Aceleração", "Pista customizada"};
constexpr fsae::sim::Event kEventValues[] = {fsae::sim::Event::Autocross, fsae::sim::Event::Skidpad,
                                             fsae::sim::Event::Acceleration, fsae::sim::Event::Custom};

// --- Ajudantes de formulário (tabela de 2 colunas: rótulo | campo) ---

bool begin_form(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("rotulo", ImGuiTableColumnFlags_WidthStretch, 1.25f);
    ImGui::TableSetupColumn("valor", ImGuiTableColumnFlags_WidthStretch, 1.0f);
    return true;
}

void row_label(const char* label, const char* tip) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(label);
    if (tip && ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

bool num(const char* label, double& v, const char* tip = nullptr, const char* fmt = "%.6g") {
    row_label(label, tip);
    ImGui::PushID(label);
    const bool changed = ImGui::InputDouble("##v", &v, 0.0, 0.0, fmt);
    ImGui::PopID();
    return changed;
}

std::string sanitize(std::string s) {
    for (char& c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '_';
    }
    return s;
}

/// Carrega o tipo de prova a partir do nome do arquivo, como a CLI.
int event_for_track(const std::string& name) {
    if (name == "skidpad") return 1;
    if (name == "acceleration") return 2;
    return 0;
}

}  // namespace

// ---------------------------------------------------------------------------
// Canais
// ---------------------------------------------------------------------------

const std::vector<ChannelInfo>& channels() {
    static const std::vector<ChannelInfo> c = {
        {"time", "Tempo", "s", false},
        {"distance", "Distância", "m", false},
        {"velocity", "Velocidade", "km/h", false},
        {"acceleration", "Acel. longitudinal", "g", false},
        {"lateral_accel", "Acel. lateral", "g", false},
        {"rpm", "Rotação do motor", "rpm", false},
        {"gear", "Marcha", "", true},
        {"engine_force", "Força do motor", "N", false},
        {"brake_force", "Força de freio", "N", false},
        {"drag", "Arrasto", "N", false},
        {"downforce", "Downforce", "N", false},
        {"rolling_resistance", "Resist. ao rolamento", "N", false},
        {"slope_force", "Força da rampa", "N", false},
        {"load_front_dist", "Carga no eixo dianteiro", "%", false},
        {"brake_temp", "Temperatura do disco", "°C", false},
        {"h_coef", "Coef. de convecção do disco", "W/m²K", false},
        {"P_conv", "Calor por convecção", "W", false},
        {"P_rad", "Calor por radiação", "W", false},
        {"P_cond", "Calor por condução", "W", false},
        {"fuel_consumed_g", "Combustível acumulado", "g", false},
    };
    return c;
}

int channel_index(const char* key) {
    const auto& c = channels();
    for (std::size_t i = 0; i < c.size(); ++i) {
        if (std::string(c[i].key) == key) return static_cast<int>(i);
    }
    return 0;
}

namespace {

std::vector<double> scaled(const std::vector<double>& v, double k) {
    std::vector<double> out(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) out[i] = v[i] * k;
    return out;
}

std::vector<std::vector<double>> build_channels(const fsae::sim::Telemetry& h) {
    std::vector<std::vector<double>> c;
    c.reserve(channels().size());
    for (const auto& ch : channels()) {
        const std::string k = ch.key;
        if (k == "time") c.push_back(h.time);
        else if (k == "distance") c.push_back(h.distance);
        else if (k == "velocity") c.push_back(scaled(h.velocity, 3.6));
        else if (k == "acceleration") c.push_back(scaled(h.acceleration, 1 / 9.81));
        else if (k == "lateral_accel") c.push_back(scaled(h.lateral_accel, 1 / 9.81));
        else if (k == "rpm") c.push_back(h.rpm);
        else if (k == "gear") c.emplace_back(h.gear.begin(), h.gear.end());
        else if (k == "engine_force") c.push_back(h.engine_force);
        else if (k == "brake_force") c.push_back(h.brake_force);
        else if (k == "drag") c.push_back(h.drag);
        else if (k == "downforce") c.push_back(h.downforce);
        else if (k == "rolling_resistance") c.push_back(h.rolling_resistance);
        else if (k == "slope_force") c.push_back(h.slope_force);
        else if (k == "load_front_dist") c.push_back(h.load_front_dist);
        else if (k == "brake_temp") c.push_back(h.brake_temp);
        else if (k == "h_coef") c.push_back(h.h_coef);
        else if (k == "P_conv") c.push_back(h.p_conv);
        else if (k == "P_rad") c.push_back(h.p_rad);
        else if (k == "P_cond") c.push_back(h.p_cond);
        else if (k == "fuel_consumed_g") c.push_back(h.fuel_consumed_g);
    }
    return c;
}

}  // namespace

// ---------------------------------------------------------------------------
// App
// ---------------------------------------------------------------------------

App::App() {
    spec_ = fsae::CarSpec::matlab_default();
    for (const char* k : {"velocity", "acceleration", "lateral_accel", "rpm", "gear"}) {
        tel_selected_[static_cast<std::size_t>(channel_index(k))] = true;
    }
    map_channel_ = channel_index("velocity");
    xy_x_ = channel_index("velocity");
    xy_y_ = channel_index("rpm");
    refresh_lists();
    load_track_by_name("autox");
    validate_car();
}

ImVec4 App::run_color(const Run& r) const {
    return hex(kRunColors[static_cast<std::size_t>(r.color_slot) % kRunColors.size()]);
}

void App::refresh_lists() {
    track_files_.clear();
    car_files_.clear();
    std::error_code ec;
    if (const auto d = find_data_subdir("tracks"); !d.empty()) {
        for (const auto& e : fs::directory_iterator(d, ec)) {
            if (e.path().extension() == ".csv") track_files_.push_back(e.path());
        }
    }
    if (const auto d = find_data_subdir("cars"); !d.empty()) {
        for (const auto& e : fs::directory_iterator(d, ec)) {
            if (e.path().extension() == ".json") car_files_.push_back(e.path());
        }
    }
    std::sort(track_files_.begin(), track_files_.end());
    std::sort(car_files_.begin(), car_files_.end());
}

void App::load_track_by_name(const std::string& name) {
    for (const auto& p : track_files_) {
        if (p.stem().string() == name) {
            load_track(p);
            return;
        }
    }
    if (!track_files_.empty() && track_.segments.empty()) load_track(track_files_.front());
}

void App::load_track(const fs::path& p) {
    try {
        track_ = fsae::track::load_track(p);
        track_file_ = to_utf8(p);
        track_error_.clear();
        event_ = event_for_track(track_.name);
        if (laps_auto_) laps_ = fsae::sim::default_laps(kEventValues[event_]);
        start_segment_ = 1;
    } catch (const std::exception& e) {
        track_error_ = e.what();
    }
}

void App::load_car(const fs::path& p) {
    try {
        spec_ = fsae::io::load_car_spec(p);
        car_file_ = to_utf8(p);
        validate_car();
        status_ = "Carro carregado: " + car_file_;
    } catch (const std::exception& e) {
        car_error_ = e.what();
        car_valid_ = false;
    }
}

void App::validate_car() {
    try {
        (void)fsae::Car::from_spec(spec_);
        car_valid_ = true;
        car_error_.clear();
    } catch (const std::exception& e) {
        car_valid_ = false;
        car_error_ = e.what();
    }
}

Run* App::current_run() { return runs_.empty() ? nullptr : &runs_.back(); }

int App::max_laps() const {
    int n = 1;
    for (const auto& r : runs_) n = std::max(n, r.result.laps);
    return n;
}

void App::run_simulation(bool replace_last) {
    run_error_.clear();
    validate_car();
    if (!car_valid_) {
        run_error_ = "Carro inválido: " + car_error_;
        return;
    }
    if (track_.segments.empty()) {
        run_error_ = "Escolha uma pista.";
        return;
    }
    try {
        Run r;
        r.spec = spec_;
        r.track_name = track_.name;
        r.car_name = spec_.name;
        r.event = kEvents[event_];
        r.segments = fsae::track::rotate_to_start(track_.segments, start_segment_);
        const fsae::Car car = fsae::Car::from_spec(spec_);
        fsae::sim::SimulationOptions opt;
        opt.laps = laps_;
        const auto t0 = std::chrono::steady_clock::now();
        r.result = fsae::sim::simulate(car, r.segments, opt);
        r.sim_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        r.geometry = fsae::track::build_geometry(r.segments);
        r.channels = build_channels(r.result.history);
        r.lap.assign(r.result.history.size(), 1);
        for (std::size_t k = 1; k < r.result.lap_start_index.size(); ++k) {
            for (std::size_t i = r.result.lap_start_index[k]; i < r.lap.size(); ++i) {
                r.lap[i] = static_cast<int>(k) + 1;
            }
        }
        for (std::size_t k = 0; k < r.segments.size(); ++k) {
            if (r.segments[k].is_corner() && !(r.result.corner_speeds[k].v_max > 0)) {
                r.zero_speed_corners.push_back(k + 1);
            }
        }

        if (replace_last && !runs_.empty()) {
            const Run& old = runs_.back();
            r.id = old.id;
            r.label = old.label;
            r.color_slot = old.color_slot;
            r.visible = old.visible;
            runs_.back() = std::move(r);
        } else {
            if (static_cast<int>(runs_.size()) >= kMaxRuns) runs_.erase(runs_.begin());
            std::array<bool, kMaxRuns> used{};
            for (const auto& o : runs_) used[static_cast<std::size_t>(o.color_slot)] = true;
            r.color_slot = static_cast<int>(std::find(used.begin(), used.end(), false) - used.begin());
            r.id = next_run_id_++;
            char label[160];
            std::snprintf(label, sizeof label, "#%d %s - %s", r.id, r.track_name.c_str(), r.car_name.c_str());
            r.label = label;
            runs_.push_back(std::move(r));
        }
        ++view_gen_;
        status_.clear();
    } catch (const std::exception& e) {
        run_error_ = e.what();
    }
}

void App::save_results(const Run& r) {
    try {
        fs::path base = find_data_subdir("tracks");
        base = base.empty() ? fs::current_path() / "results" : base.parent_path().parent_path() / "results";
        const fs::path out = base / sanitize(r.label);
        fsae::io::RunInfo info;
        info.car_name = r.car_name;
        info.track_name = r.track_name;
        info.event = r.event;
        fsae::io::write_telemetry_csv(out / "telemetry.csv", r.result);
        fsae::io::write_summary_json(out / "summary.json", r.result, info);
        fsae::io::write_segments_csv(out / "track.csv", r.segments, r.result);
        fsae::io::write_geometry_csv(out / "geometry.csv", r.geometry);
        fsae::io::save_car_spec(r.spec, out / "car.json");
        status_ = "Resultados gravados em " + to_utf8(out);
        open_folder(out);
    } catch (const std::exception& e) {
        status_ = std::string("Erro ao gravar: ") + e.what();
    }
}

// ---------------------------------------------------------------------------
// Quadro
// ---------------------------------------------------------------------------

void App::frame() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("fsae_lapsim", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    const float left_w = std::max(380.0f, vp->WorkSize.x * 0.27f);
    ImGui::BeginChild("esquerda", ImVec2(left_w, 0), ImGuiChildFlags_Border);
    panel_setup();
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("direita", ImVec2(0, 0), ImGuiChildFlags_None);

    struct Tab { const char* name; void (App::*fn)(); };
    const Tab tabs[] = {
        {"Telemetria", &App::tab_telemetry}, {"Mapa da pista", &App::tab_track_map},
        {"Diagrama G-G", &App::tab_gg},      {"Dispersão X-Y", &App::tab_xy},
        {"Curvas", &App::tab_corners},       {"Comparação", &App::tab_compare},
        {"Motor", &App::tab_engine},         {"Execuções", &App::tab_runs},
    };
    if (ImGui::BeginTabBar("abas")) {
        for (const auto& t : tabs) {
            ImGuiTabItemFlags flags = 0;
            if (!forced_tab_.empty() && std::string(t.name).rfind(forced_tab_, 0) == 0) flags |= ImGuiTabItemFlags_SetSelected;
            if (ImGui::BeginTabItem(t.name, nullptr, flags)) {
                (this->*t.fn)();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    forced_tab_.clear();
    ImGui::EndChild();
    ImGui::End();
}

void App::panel_setup() {
    bool changed = false;

    ImGui::SeparatorText("Pista e prova");
    changed |= track_selector();

    ImGui::SeparatorText("Simulação");
    ImGui::BeginDisabled(!car_valid_ || track_.segments.empty());
    if (ImGui::Button("Rodar simulação", ImVec2(-FLT_MIN, 36)) ||
        (ImGui::IsKeyPressed(ImGuiKey_F5) && car_valid_)) {
        run_simulation(false);
    }
    ImGui::EndDisabled();
    ImGui::Checkbox("Atualizar automaticamente ao editar", &auto_run_);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Cada alteração no carro ou na pista refaz a última execução.\n"
                          "Use \"Rodar simulação\" (F5) para guardar uma nova execução e comparar.");
    }
    if (!runs_.empty()) {
        ImGui::SameLine();
        if (ImGui::SmallButton("Limpar execuções")) {
            runs_.clear();
            ++view_gen_;
        }
    }
    if (!run_error_.empty()) ImGui::TextColored(hex(0xe66767), "%s", run_error_.c_str());

    panel_results();
    if (!status_.empty()) ImGui::TextWrapped("%s", status_.c_str());

    ImGui::SeparatorText("Carro");
    changed |= car_editor();

    if (changed) {
        ++view_gen_;
        validate_car();
        if (auto_run_ && car_valid_ && !track_.segments.empty()) run_simulation(!runs_.empty());
    }
}

bool App::needs_fit(int tab) {
    auto& seen = fitted_gen_[static_cast<std::size_t>(tab)];
    if (seen == view_gen_) return false;
    seen = view_gen_;
    return true;
}

bool App::car_editor() {
    bool ch = false;
    auto& s = spec_;

    // Arquivo
    const std::string current = car_file_.empty() ? "(padrão do MATLAB)" : fs::path(car_file_).filename().string();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##carro", current.c_str())) {
        if (ImGui::Selectable("Carro padrão (initializeCar.m)")) {
            spec_ = fsae::CarSpec::matlab_default();
            car_file_.clear();
            ch = true;
        }
        for (const auto& p : car_files_) {
            if (ImGui::Selectable(p.filename().string().c_str())) {
                load_car(p);
                ch = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Abrir...")) {
        if (auto p = open_file_dialog("Abrir carro", "Carro JSON", "*.json")) {
            load_car(*p);
            ch = true;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Salvar como...")) {
        const std::string def = sanitize(s.name) + ".json";
        if (auto p = save_file_dialog("Salvar carro", "Carro JSON", "*.json", def.c_str())) {
            try {
                fsae::io::save_car_spec(s, *p);
                car_file_ = to_utf8(*p);
                status_ = "Carro salvo em " + car_file_;
                refresh_lists();
            } catch (const std::exception& e) {
                status_ = e.what();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Restaurar padrão")) {
        spec_ = fsae::CarSpec::matlab_default();
        car_file_.clear();
        ch = true;
    }

    ImGui::SetNextItemWidth(-FLT_MIN);
    ch |= ImGui::InputTextWithHint("##nome", "nome do carro", &s.name);

    if (ImGui::CollapsingHeader("Massa", ImGuiTreeNodeFlags_DefaultOpen) && begin_form("massa")) {
        ch |= num("Carro [kg]", s.car_mass_kg, "mass_only");
        ch |= num("Piloto [kg]", s.driver_mass_kg, "pilot_mass");
        ch |= num("Gravidade [m/s²]", s.gravity_m_s2, "g");
        ImGui::EndTable();
    }
    if (ImGui::CollapsingHeader("Aerodinâmica", ImGuiTreeNodeFlags_DefaultOpen) && begin_form("aero")) {
        ch |= num("Área frontal [m²]", s.frontal_area_m2, "frontalArea");
        ch |= num("Densidade do ar [kg/m³]", s.air_density_kg_m3, "rho");
        ch |= num("Cl*A [m²]", s.cl_a_m2, "Coeficiente de sustentação vezes área (cl = ClA / A)");
        ch |= num("Cd*A [m²]", s.cd_a_m2, "Coeficiente de arrasto vezes área (cd = CdA / A)");
        ch |= num("Centro de pressão [% traseira]", s.center_of_pressure_rear_percent, "cpaero");
        ImGui::EndTable();
    }
    if (ImGui::CollapsingHeader("Chassi e pneus", ImGuiTreeNodeFlags_DefaultOpen) && begin_form("chassi")) {
        ch |= num("Entre-eixos [m]", s.wheelbase_m, "wheelbase");
        ch |= num("Raio do pneu [mm]", s.tire_radius_mm, "tire_radius");
        ch |= num("Distribuição de peso [% traseira]", s.weight_dist_rear_percent, "weight_dist_rear_percent");
        ch |= num("Altura do CG [m]", s.cg_height_m, "cg_height");
        ch |= num("Bitola dianteira [m]", s.track_front_m, "track_front_m");
        ch |= num("Bitola traseira [m]", s.track_rear_m, "track_rear_m");
        ch |= num("Atrito longitudinal estático", s.mu_x_static, "mux_static");
        ch |= num("Atrito longitudinal cinético", s.mu_x_kinetic, "mux_kinetic (quando a roda patina)");
        ch |= num("Atrito lateral", s.mu_y, "muy");
        ImGui::EndTable();
    }
    if (ImGui::CollapsingHeader("Transmissão") ) {
        if (begin_form("trans")) {
            ch |= num("Redução primária", s.primary_reduction);
            ch |= num("Redução final", s.final_drive);
            ch |= num("Eficiência", s.powertrain_efficiency, "powertrain_efficiency (0 a 1)");
            ch |= num("Tempo de troca [ms]", s.shift_time_ms, "shiftTime");
            ch |= num("RPM de largada (two-step)", s.launch_rpm, "launchRPM");
            ch |= num("RPM de redução", s.downshift_rpm, "downshiftRPM");
            row_label("Número de marchas", nullptr);
            int n = static_cast<int>(s.gear_ratios.size());
            if (ImGui::InputInt("##nmarchas", &n) && n >= 1 && n <= 10) {
                const double last_ratio = s.gear_ratios.empty() ? 1.0 : s.gear_ratios.back();
                const double last_rpm = s.upshift_rpm.empty() ? 12000.0 : s.upshift_rpm.back();
                s.gear_ratios.resize(static_cast<std::size_t>(n), last_ratio);
                s.upshift_rpm.resize(static_cast<std::size_t>(n), last_rpm);
                ch = true;
            }
            ImGui::EndTable();
        }
        if (ImGui::BeginTable("marchas", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Marcha", ImGuiTableColumnFlags_WidthFixed, 60);
            ImGui::TableSetupColumn("Relação");
            ImGui::TableSetupColumn("RPM de subida");
            ImGui::TableHeadersRow();
            for (std::size_t g = 0; g < s.gear_ratios.size(); ++g) {
                ImGui::PushID(static_cast<int>(g));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu", g + 1);
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ch |= ImGui::InputDouble("##r", &s.gear_ratios[g], 0, 0, "%.4g");
                ImGui::TableSetColumnIndex(2);
                ImGui::SetNextItemWidth(-FLT_MIN);
                if (g < s.upshift_rpm.size()) ch |= ImGui::InputDouble("##u", &s.upshift_rpm[g], 0, 0, "%.0f");
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    if (ImGui::CollapsingHeader("Motor (curvas)")) {
        auto table = [&](const char* id, const char* cx, const char* cy, std::vector<double>& x,
                         std::vector<double>& y) {
            ImGui::PushID(id);
            if (ImGui::BeginTable("t", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                              ImGuiTableFlags_ScrollY,
                                  ImVec2(0, 200))) {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn(cx);
                ImGui::TableSetupColumn(cy);
                ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 24);
                ImGui::TableHeadersRow();
                std::size_t remove = x.size();
                for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ch |= ImGui::InputDouble("##x", &x[i], 0, 0, "%.6g");
                    ImGui::TableSetColumnIndex(1);
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    ch |= ImGui::InputDouble("##y", &y[i], 0, 0, "%.6g");
                    ImGui::TableSetColumnIndex(2);
                    if (ImGui::SmallButton("x")) remove = i;
                    ImGui::PopID();
                }
                ImGui::EndTable();
                if (remove < x.size() && x.size() > 2) {
                    x.erase(x.begin() + static_cast<std::ptrdiff_t>(remove));
                    y.erase(y.begin() + static_cast<std::ptrdiff_t>(remove));
                    ch = true;
                }
            }
            if (ImGui::SmallButton("+ ponto")) {
                const double nx = x.empty() ? 1000 : x.back() + (x.size() > 1 ? x.back() - x[x.size() - 2] : 500);
                x.push_back(nx);
                y.push_back(y.empty() ? 0 : y.back());
                ch = true;
            }
            ImGui::PopID();
        };
        ImGui::TextDisabled("Torque (interpolação pchip, como o dataEngine.m)");
        table("torque", "rpm", "Torque [N.m]", s.engine_rpm, s.engine_torque_nm);
        ImGui::TextDisabled("Consumo específico (BSFC, interpolação linear)");
        table("bsfc", "rpm", "BSFC [g/kWh]", s.bsfc_rpm, s.bsfc_g_kwh);
    }
    if (ImGui::CollapsingHeader("Freio e ar") && begin_form("freio")) {
        ch |= num("Massa do disco [kg]", s.brake_disc_mass_kg, "brake_mass");
        ch |= num("Diâmetro do disco [m]", s.brake_disc_diameter_m, "brake_diameter");
        ch |= num("Área exposta do disco [m²]", s.brake_disc_surface_area_m2, "brake_surface_area");
        ch |= num("Condutividade do disco [W/m.K]", s.brake_disc_thermal_conductivity_w_mk, "brake_thermalcond");
        ch |= num("Emissividade do disco", s.brake_disc_emissivity, "brake_emissivity");
        ch |= num("Número de bobbins", s.brake_bobbin_count);
        ch |= num("Diâmetro do bobbin [m]", s.brake_bobbin_diameter_m);
        ch |= num("Distância de condução [m]", s.brake_conduction_distance_m, "brake_cond_dist");
        ch |= num("Condutividade do ar [W/m.K]", s.air_thermal_conductivity_w_mk, "air_k");
        ch |= num("Viscosidade do ar [Pa.s]", s.air_dynamic_viscosity_pa_s, "air_mu", "%.4g");
        ImGui::EndTable();
    }
    if (ch) validate_car();
    if (!car_valid_) ImGui::TextColored(hex(0xe66767), "%s", car_error_.c_str());
    return ch;
}

bool App::track_selector() {
    bool ch = false;
    ImGui::SetNextItemWidth(-FLT_MIN);
    const std::string current = track_.name.empty() ? "(escolha uma pista)" : track_.name;
    if (ImGui::BeginCombo("##pista", current.c_str())) {
        for (const auto& p : track_files_) {
            if (ImGui::Selectable(p.stem().string().c_str(), p.stem().string() == track_.name)) {
                load_track(p);
                ch = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::Button("Abrir pista...")) {
        if (auto p = open_file_dialog("Abrir pista", "Pista CSV", "*.csv")) {
            load_track(*p);
            ch = true;
        }
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Formatos: CSV do projeto, matriz segments do MATLAB\n"
                          "(index,length,radius,slope,type) ou Tipo;Comprimento;Raio do trackExcel.");
    }
    if (!track_error_.empty()) ImGui::TextColored(hex(0xe66767), "%s", track_error_.c_str());
    if (!track_.segments.empty()) {
        std::size_t corners = 0;
        for (const auto& sg : track_.segments) corners += sg.is_corner() ? 1 : 0;
        ImGui::SameLine();
        ImGui::TextDisabled("%zu segmentos, %zu curvas, %.1f m", track_.segments.size(), corners,
                            track_.length());
    }
    if (begin_form("prova")) {
        row_label("Prova", "Define o número padrão de voltas");
        if (ImGui::Combo("##prova", &event_, kEvents, IM_ARRAYSIZE(kEvents))) {
            if (laps_auto_) laps_ = fsae::sim::default_laps(kEventValues[event_]);
            ch = true;
        }
        row_label("Voltas", "A primeira volta parte parada. Endurance: autocross com mais voltas.");
        if (ImGui::InputInt("##voltas", &laps_)) {
            laps_ = std::clamp(laps_, 1, 100);
            laps_auto_ = false;
            ch = true;
        }
        row_label("Segmento de largada", "A volta começa neste segmento (circshift do MATLAB)");
        const int n = std::max<int>(1, static_cast<int>(track_.segments.size()));
        if (ImGui::InputInt("##largada", &start_segment_)) {
            start_segment_ = std::clamp(start_segment_, 1, n);
            ch = true;
        }
        ImGui::EndTable();
    }
    return ch;
}

void App::panel_results() {
    Run* r = current_run();
    if (!r) {
        ImGui::TextDisabled("Nenhuma execução ainda. Clique em \"Rodar simulação\".");
        return;
    }
    const auto& res = r->result;
    ImGui::SeparatorText("Resultado");
    ImGui::ColorButton("##cor", run_color(*r), ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
    ImGui::SameLine();
    ImGui::TextUnformatted(r->label.c_str());
    if (ImGui::BeginTable("res", 2, ImGuiTableFlags_RowBg)) {
        auto line = [](const char* k, const char* fmt, double v) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(k);
            ImGui::TableSetColumnIndex(1);
            ImGui::Text(fmt, v);
        };
        for (int k = 0; k < res.laps; ++k) {
            char name[48];
            std::snprintf(name, sizeof name, "Volta %d%s", k + 1,
                          res.laps > 1 ? (k == 0 ? " (partida)" : " (lançada)") : "");
            line(name, "%.3f s", res.lap_times[static_cast<std::size_t>(k)]);
        }
        if (res.laps > 1) line("Volta mais rápida", "%.3f s", res.fastest_lap);
        line("Tempo total", "%.3f s", res.total_time);
        line("Velocidade máxima", "%.2f km/h", res.max_velocity_kmh);
        line("Acel. lateral máxima", "%.3f g", res.max_lat_g);
        line("Temp. máxima do disco", "%.1f °C", res.max_brake_temp);
        line("Consumo total", "%.2f g", res.total_fuel_g);
        line("Tempo de cálculo", "%.2f ms", r->sim_ms);
        ImGui::EndTable();
    }
    if (!r->zero_speed_corners.empty()) {
        ImGui::TextColored(hex(0xc98500),
                           "AVISO: %zu curva(s) com velocidade máxima 0 (raio grande).\n"
                           "O modelo do MATLAB zera a aderência acima de ~136,5 m\n"
                           "(KNOWN_ISSUES, item 2): o tempo de volta não é confiável.",
                           r->zero_speed_corners.size());
    }
    if (ImGui::Button("Salvar resultados (CSV/JSON)")) save_results(*r);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Grava telemetry.csv, summary.json, track.csv, geometry.csv e car.json\n"
                          "em results/<execução> (mesmo formato do fsae_sim run).");
    }
}

Series App::series(const Run& r, int channel, int lap, bool x_is_distance) const {
    Series s;
    const auto& y = r.channels[static_cast<std::size_t>(channel)];
    const auto& t = r.channels[0];
    const auto& d = r.channels[1];
    const int k = lap < 0 ? r.result.laps : lap;
    if (k > r.result.laps) return s;
    double t0 = 0, d0 = 0;
    bool first = true;
    for (std::size_t i = 0; i < y.size(); ++i) {
        if (k > 0 && r.lap[i] != k) continue;
        if (first && k > 1) {
            t0 = t[i];
            d0 = static_cast<double>(k - 1) * r.result.track_length;
        }
        first = false;
        s.x.push_back(x_is_distance ? d[i] - d0 : t[i] - t0);
        // Os próprios canais de tempo e distância também começam em zero na volta.
        s.y.push_back(channel == 0 ? y[i] - t0 : channel == 1 ? y[i] - d0 : y[i]);
    }
    return s;
}

Series App::series_xy(const Run& r, int cx, int cy, int lap) const {
    Series s;
    const auto& x = r.channels[static_cast<std::size_t>(cx)];
    const auto& y = r.channels[static_cast<std::size_t>(cy)];
    const int k = lap < 0 ? r.result.laps : lap;
    for (std::size_t i = 0; i < y.size(); ++i) {
        if (k > 0 && r.lap[i] != k) continue;
        s.x.push_back(x[i]);
        s.y.push_back(y[i]);
    }
    return s;
}

}  // namespace fsae::gui
