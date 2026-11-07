#include "app.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <exception>
#include <fstream>
#include <sstream>

#include <imgui.h>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>

#include "fsae/io/car_json.hpp"
#include "fsae/io/results.hpp"
#include "fsae/version.hpp"
#include "platform.hpp"

namespace fs = std::filesystem;

namespace fsae::gui {

namespace {

constexpr int kMaxRuns = 8;

const char* const kEvents[] = {"Autocross", "Skidpad", "Aceleração", "Pista customizada"};
constexpr fsae::sim::Event kEventValues[] = {fsae::sim::Event::Autocross, fsae::sim::Event::Skidpad,
                                             fsae::sim::Event::Acceleration, fsae::sim::Event::Custom};

std::string sanitize(std::string s) {
    for (char& c : s) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_')) c = '_';
    }
    return s;
}

int event_for_track(const std::string& name) {
    if (name == "skidpad") return 1;
    if (name == "acceleration") return 2;
    return 0;
}

std::vector<double> scaled(const std::vector<double>& v, double k) {
    std::vector<double> out(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) out[i] = v[i] * k;
    return out;
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
// Construção, preferências
// ---------------------------------------------------------------------------

App::App() {
    spec_ = fsae::CarSpec::matlab_default();
    mark_car_saved();
    for (const char* k : {"velocity", "acceleration", "lateral_accel", "rpm", "gear"}) {
        tel_selected_[static_cast<std::size_t>(channel_index(k))] = true;
    }
    map_channel_ = channel_index("velocity");
    xy_x_ = channel_index("velocity");
    xy_y_ = channel_index("rpm");
    load_settings();
    apply_theme(theme_);
    refresh_lists();
    load_track_by_name("autox");
    validate_car();
}

App::~App() {
    if (persist_) save_settings();
}

void App::set_theme(Theme t) {
    theme_ = t;
    apply_theme(t);
}

void App::load_settings() {
    std::ifstream in(settings_path());
    std::string line;
    while (std::getline(in, line)) {
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "theme") theme_ = v == "dark" ? Theme::Dark : Theme::Light;
        else if (k == "map_colormap") {
            for (int i = 0; i < colormap_count(); ++i) {
                if (v == colormap_name(i)) map_cmap_ = i;
            }
        } else if (k == "map_invert") map_invert_ = v == "1";
        else if (k == "map_width") map_width_ = std::clamp(static_cast<float>(std::atof(v.c_str())), 2.0f, 20.0f);
        else if (k == "auto_run") auto_run_ = v == "1";
    }
}

void App::save_settings() const {
    std::error_code ec;
    const fs::path p = settings_path();
    fs::create_directories(p.parent_path(), ec);
    std::ofstream out(p);
    if (!out) return;
    out << "theme=" << (theme_ == Theme::Dark ? "dark" : "light") << "\n"
        << "map_colormap=" << colormap_name(map_cmap_) << "\n"
        << "map_invert=" << (map_invert_ ? 1 : 0) << "\n"
        << "map_width=" << map_width_ << "\n"
        << "auto_run=" << (auto_run_ ? 1 : 0) << "\n";
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
    car_names_.clear();
    for (const auto& p : car_files_) {
        try {
            car_names_.push_back(fsae::io::load_car_spec(p).name);
        } catch (const std::exception&) {
            car_names_.push_back("(arquivo inválido)");
        }
    }
}

// ---------------------------------------------------------------------------
// Carro
// ---------------------------------------------------------------------------

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

bool App::car_modified() const { return fsae::io::car_spec_to_json(spec_) != saved_json_; }

void App::mark_car_saved() { saved_json_ = fsae::io::car_spec_to_json(spec_); }

void App::on_car_changed() {
    validate_car();
    ++view_gen_;
    if (auto_run_ && car_valid_ && !track_.segments.empty()) run_simulation(!runs_.empty());
}

void App::request_car_action(CarAction a, const fs::path& p) {
    if (a != CarAction::Duplicate && car_modified()) {
        pending_action_ = a;
        pending_path_ = p;
        open_discard_modal_ = true;
        return;
    }
    do_car_action(a, p);
}

void App::do_car_action(CarAction a, const fs::path& p) {
    switch (a) {
        case CarAction::None: return;
        case CarAction::New:
            spec_ = fsae::CarSpec::matlab_default();
            spec_.name = "Novo carro";
            car_file_.clear();
            saved_json_.clear();  // ainda não salvo
            show_editor_ = true;
            forced_editor_tab_ = "Geral";
            status_ = "Novo carro criado a partir do carro padrão. Edite os dados e salve.";
            break;
        case CarAction::Default:
            spec_ = fsae::CarSpec::matlab_default();
            car_file_.clear();
            mark_car_saved();
            status_ = "Carro padrão (initializeCar.m) carregado.";
            break;
        case CarAction::Load:
            try {
                spec_ = fsae::io::load_car_spec(p);
                car_file_ = to_utf8(p);
                mark_car_saved();
                status_ = "Carro carregado: " + to_utf8(p.filename());
            } catch (const std::exception& e) {
                status_ = std::string("Erro ao abrir o carro: ") + e.what();
            }
            break;
        case CarAction::Duplicate:
            spec_.name += " (cópia)";
            car_file_.clear();
            saved_json_.clear();
            status_ = "Cópia criada. Salve com \"Salvar como...\" para guardá-la.";
            break;
    }
    on_car_changed();
}

void App::confirm_discard_modal() {
    if (open_discard_modal_) {
        ImGui::OpenPopup("Descartar alterações?");
        open_discard_modal_ = false;
    }
    if (ImGui::BeginPopupModal("Descartar alterações?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("O carro \"%s\" tem alterações que não foram salvas.", spec_.name.c_str());
        ImGui::Spacing();
        if (primary_button("Salvar e continuar")) {
            if (save_car(false)) {
                do_car_action(pending_action_, pending_path_);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Descartar")) {
            do_car_action(pending_action_, pending_path_);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar")) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

bool App::save_car(bool save_as) {
    fs::path target = car_file_.empty() ? fs::path() : fs::u8path(car_file_);
    if (save_as || target.empty()) {
        const std::string def = sanitize(spec_.name) + ".json";
        auto p = save_file_dialog("Salvar carro", "Carro JSON", "*.json", def.c_str());
        if (!p) {
            // Sem diálogo (fora do Windows): grava em data/cars
            const auto dir = find_data_subdir("cars");
            if (dir.empty() || !save_as) return false;
            p = dir / def;
        }
        target = *p;
    }
    try {
        fsae::io::save_car_spec(spec_, target);
        car_file_ = to_utf8(target);
        mark_car_saved();
        refresh_lists();
        status_ = "Carro salvo em " + car_file_;
        return true;
    } catch (const std::exception& e) {
        status_ = std::string("Erro ao salvar: ") + e.what();
        return false;
    }
}

void App::open_car_dialog() {
    if (auto p = open_file_dialog("Abrir carro", "Carro JSON", "*.json")) request_car_action(CarAction::Load, *p);
}

void App::import_from_file(ImportTarget t) {
    if (auto p = open_file_dialog("Importar dados", "Planilhas e CSV", "*.xlsx;*.xlsm;*.csv;*.txt")) {
        import_.open_file(t, *p);
    }
}

void App::import_from_clipboard(ImportTarget t) {
    const char* text = ImGui::GetClipboardText();
    if (!text || !*text) {
        status_ = "A área de transferência está vazia. Copie as colunas no Excel (Ctrl+C) e tente de novo.";
        return;
    }
    import_.open_text(t, text);
}

// ---------------------------------------------------------------------------
// Pista
// ---------------------------------------------------------------------------

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

void App::open_track_dialog() {
    if (auto p = open_file_dialog("Abrir pista", "Pista CSV", "*.csv")) load_track(*p);
}

// ---------------------------------------------------------------------------
// Simulação
// ---------------------------------------------------------------------------

Run* App::current_run() { return runs_.empty() ? nullptr : &runs_.back(); }

int App::max_laps() const {
    int n = 1;
    for (const auto& r : runs_) n = std::max(n, r.result.laps);
    return n;
}

bool App::needs_fit(int tab) {
    auto& seen = fitted_gen_[static_cast<std::size_t>(tab)];
    if (seen == view_gen_) return false;
    seen = view_gen_;
    return true;
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
    menu_bar();

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    ImGui::Begin("fsae_lapsim", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoSavedSettings);

    const float left_w = std::max(380.0f, vp->WorkSize.x * 0.26f);
    ImGui::BeginChild("esquerda", ImVec2(left_w, 0), ImGuiChildFlags_Border);
    if (ImGui::BeginTabBar("painel")) {
        const auto flag = [&](const char* name) {
            return !forced_left_tab_.empty() && std::string(name).rfind(forced_left_tab_, 0) == 0
                       ? ImGuiTabItemFlags_SetSelected
                       : 0;
        };
        if (ImGui::BeginTabItem("Simulação", nullptr, flag("Simulação"))) {
            ImGui::BeginChild("sim_scroll");
            panel_simulation();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        const std::string cars_tab = std::string("Carros") + (car_modified() ? " *" : "") + "###carros";
        if (ImGui::BeginTabItem(cars_tab.c_str(), nullptr, flag("Carros"))) {
            ImGui::BeginChild("car_scroll");
            panel_cars();
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    forced_left_tab_.clear();
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

    car_editor_window();
    confirm_discard_modal();
    if (import_.draw(spec_)) {
        status_ = "Dados importados para o carro \"" + spec_.name + "\".";
        on_car_changed();
    }
}

void App::menu_bar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("Arquivo")) {
        if (ImGui::MenuItem("Novo carro")) request_car_action(CarAction::New);
        if (ImGui::MenuItem("Abrir carro...")) open_car_dialog();
        if (ImGui::MenuItem("Salvar carro", "Ctrl+S")) save_car(false);
        if (ImGui::MenuItem("Salvar carro como...")) save_car(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Abrir pista...")) open_track_dialog();
        ImGui::Separator();
        if (ImGui::MenuItem("Salvar resultados da execução atual", nullptr, false, !runs_.empty())) {
            save_results(runs_.back());
        }
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Carro")) {
        if (ImGui::MenuItem("Editar dados do carro...", "F2")) show_editor_ = true;
        if (ImGui::MenuItem("Importar curva de torque...")) import_from_file(ImportTarget::Torque);
        if (ImGui::MenuItem("Importar BSFC...")) import_from_file(ImportTarget::Bsfc);
        if (ImGui::MenuItem("Importar relações de marcha...")) import_from_file(ImportTarget::Gears);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Exibir")) {
        if (ImGui::MenuItem("Tema claro", nullptr, theme_ == Theme::Light)) set_theme(Theme::Light);
        if (ImGui::MenuItem("Tema escuro", nullptr, theme_ == Theme::Dark)) set_theme(Theme::Dark);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Ajuda")) {
        ImGui::Text("FSAE Lap Simulator %s", std::string(fsae::version()).c_str());
        ImGui::TextDisabled("Port C++ do simulador MATLAB da equipe (resultados idênticos).");
        ImGui::TextDisabled("F5: rodar simulação   F2: editar carro   Ctrl+S: salvar carro");
        ImGui::EndMenu();
    }
    // Carro ativo à direita da barra
    const std::string active = "Carro: " + spec_.name + (car_modified() ? " (não salvo)" : "");
    ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(active.c_str()).x - 20);
    ImGui::TextDisabled("%s", active.c_str());
    ImGui::EndMainMenuBar();

    const ImGuiIO& io = ImGui::GetIO();
    if (ImGui::IsKeyPressed(ImGuiKey_F2)) show_editor_ = true;
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) save_car(false);
}

// ---------------------------------------------------------------------------
// Painel "Simulação"
// ---------------------------------------------------------------------------

namespace {

bool begin_form(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("rotulo", ImGuiTableColumnFlags_WidthStretch, 1.1f);
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

}  // namespace

void App::panel_simulation() {
    ImGui::SeparatorText("Carro");
    ImGui::TextUnformatted(spec_.name.c_str());
    if (car_modified()) {
        ImGui::SameLine();
        ImGui::TextColored(color_warning(), "(não salvo)");
    }
    if (ImGui::Button("Trocar carro")) forced_left_tab_ = "Carros";
    ImGui::SameLine();
    if (ImGui::Button("Editar dados...")) show_editor_ = true;
    if (!car_valid_) ImGui::TextColored(color_error(), "%s", car_error_.c_str());

    ImGui::SeparatorText("Pista e prova");
    bool changed = track_selector();

    ImGui::SeparatorText("Rodar");
    ImGui::BeginDisabled(!car_valid_ || track_.segments.empty());
    if (primary_button("Rodar simulação  (F5)", ImVec2(-FLT_MIN, 38)) ||
        (ImGui::IsKeyPressed(ImGuiKey_F5) && car_valid_ && !track_.segments.empty())) {
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
    if (!run_error_.empty()) ImGui::TextColored(color_error(), "%s", run_error_.c_str());

    panel_results();
    if (!status_.empty()) {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", status_.c_str());
    }
    if (changed) on_car_changed();
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
        open_track_dialog();
        ch = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Formatos: CSV do projeto, matriz segments do MATLAB\n"
                          "(index,length,radius,slope,type) ou Tipo;Comprimento;Raio do trackExcel.");
    }
    if (!track_error_.empty()) ImGui::TextColored(color_error(), "%s", track_error_.c_str());
    if (!track_.segments.empty()) {
        std::size_t corners = 0;
        for (const auto& sg : track_.segments) corners += sg.is_corner() ? 1 : 0;
        ImGui::SameLine();
        ImGui::TextDisabled("%zu segmentos, %zu curvas, %.1f m", track_.segments.size(), corners, track_.length());
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
    ImGui::SeparatorText("Resultado");
    if (!r) {
        ImGui::TextDisabled("Nenhuma execução ainda. Clique em \"Rodar simulação\".");
        return;
    }
    const auto& res = r->result;
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
        ImGui::TextColored(color_warning(),
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

// ---------------------------------------------------------------------------
// Painel "Carros"
// ---------------------------------------------------------------------------

void App::panel_cars() {
    ImGui::SeparatorText("Carro ativo");
    ImGui::TextUnformatted(spec_.name.c_str());
    ImGui::TextDisabled("%s", car_file_.empty() ? "(ainda não salvo em arquivo)" : to_utf8(fs::u8path(car_file_).filename()).c_str());
    if (car_modified()) ImGui::TextColored(color_warning(), "Alterações não salvas");

    if (primary_button("Editar dados do carro...  (F2)", ImVec2(-FLT_MIN, 36))) show_editor_ = true;
    const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3;
    if (ImGui::Button("Novo", ImVec2(w, 0))) request_car_action(CarAction::New);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Começa um carro novo a partir dos valores do carro padrão.");
    ImGui::SameLine();
    if (ImGui::Button("Duplicar", ImVec2(w, 0))) request_car_action(CarAction::Duplicate);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Cria uma cópia do carro ativo para testar variações.");
    ImGui::SameLine();
    if (ImGui::Button("Abrir arquivo...", ImVec2(w, 0))) open_car_dialog();
    const float w2 = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2;
    ImGui::BeginDisabled(!car_modified() && !car_file_.empty());
    if (ImGui::Button("Salvar", ImVec2(w2, 0))) save_car(false);
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Salvar como...", ImVec2(w2, 0))) save_car(true);
    if (!car_valid_) ImGui::TextColored(color_error(), "%s", car_error_.c_str());

    ImGui::SeparatorText("Carros disponíveis");
    ImGui::TextDisabled("Arquivos .json em data/cars. Clique para usar.");
    if (ImGui::BeginListBox("##garagem", ImVec2(-FLT_MIN, 8 * ImGui::GetTextLineHeightWithSpacing()))) {
        if (ImGui::Selectable("Carro padrão (initializeCar.m)", car_file_.empty() && !car_modified())) {
            request_car_action(CarAction::Default);
        }
        for (std::size_t i = 0; i < car_files_.size(); ++i) {
            const auto& p = car_files_[i];
            const bool sel = !car_file_.empty() && fs::u8path(car_file_) == p;
            const std::string label = to_utf8(p.filename()) + "  -  " + car_names_[i];
            if (ImGui::Selectable(label.c_str(), sel)) request_car_action(CarAction::Load, p);
        }
        ImGui::EndListBox();
    }
    if (ImGui::SmallButton("Atualizar lista")) refresh_lists();
    ImGui::SameLine();
    if (const auto d = find_data_subdir("cars"); !d.empty() && ImGui::SmallButton("Abrir pasta data/cars")) open_folder(d);

    ImGui::SeparatorText("Resumo");
    car_summary();
}

}  // namespace fsae::gui
