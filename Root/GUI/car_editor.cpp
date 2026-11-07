// Janela de edição dos dados do carro (abas Geral, Aerodinâmica, Chassi e pneus,
// Transmissão, Motor, Freio e ar) e o resumo do painel "Carros".
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numeric>
#include <string>
#include <vector>

#include <imgui.h>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>

#include "app.hpp"
#include "IO_Tools/car_json.hpp"
#include "Physics/powertrain.hpp"
#include "Physics/resistances.hpp"
#include "theme.hpp"

namespace fsae::gui {

namespace {

bool begin_form(const char* id) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("rotulo", ImGuiTableColumnFlags_WidthStretch, 1.2f);
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

/// Texto fixo (valor calculado) na coluna da direita.
void info(const char* label, const char* fmt, double v, const char* tip = nullptr) {
    row_label(label, tip);
    ImGui::TextDisabled(fmt, v);
}

/// Tabela editável de dois vetores (x, y) com botões de remover e adicionar.
bool xy_table(const char* id, const char* cx, const char* cy, std::vector<double>& x, std::vector<double>& y,
              float height) {
    bool ch = false;
    ImGui::PushID(id);
    if (ImGui::BeginTable("t", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
                          ImVec2(0, height))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 30);
        ImGui::TableSetupColumn(cx);
        ImGui::TableSetupColumn(cy);
        ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 26);
        ImGui::TableHeadersRow();
        std::size_t remove = x.size();
        for (std::size_t i = 0; i < x.size() && i < y.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("%zu", i + 1);
            ImGui::TableSetColumnIndex(1);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ch |= ImGui::InputDouble("##x", &x[i], 0, 0, "%.6g");
            ImGui::TableSetColumnIndex(2);
            ImGui::SetNextItemWidth(-FLT_MIN);
            ch |= ImGui::InputDouble("##y", &y[i], 0, 0, "%.6g");
            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("x")) remove = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remover este ponto");
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
    return ch;
}

}  // namespace

// ---------------------------------------------------------------------------

void App::car_editor_window() {
    if (!show_editor_) return;
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(980.0f, vp->WorkSize.x * 0.8f), std::min(760.0f, vp->WorkSize.y * 0.88f)),
                             ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    const std::string title = "Dados do carro: " + spec_.name + (car_modified() ? " *" : "") + "###editor_carro";
    if (!ImGui::Begin(title.c_str(), &show_editor_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    bool ch = false;
    const float footer = ImGui::GetFrameHeightWithSpacing() * 2.2f;
    ImGui::BeginChild("abas_carro", ImVec2(0, -footer));
    if (ImGui::BeginTabBar("abas_editor")) {
        struct Tab { const char* name; bool (App::*fn)(); };
        const Tab tabs[] = {{"Geral", &App::editor_general},
                            {"Aerodinâmica", &App::editor_aero},
                            {"Chassi e pneus", &App::editor_chassis},
                            {"Transmissão", &App::editor_transmission},
                            {"Motor", &App::editor_engine},
                            {"Freio e ar", &App::editor_brakes}};
        for (const auto& t : tabs) {
            ImGuiTabItemFlags f = 0;
            if (!forced_editor_tab_.empty() && std::string(t.name).rfind(forced_editor_tab_, 0) == 0) {
                f = ImGuiTabItemFlags_SetSelected;
            }
            if (ImGui::BeginTabItem(t.name, nullptr, f)) {
                ImGui::BeginChild("conteudo");
                ch |= (this->*t.fn)();
                ImGui::EndChild();
                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
    forced_editor_tab_.clear();
    ImGui::EndChild();

    // Rodapé
    ImGui::Separator();
    if (!car_valid_) ImGui::TextColored(color_error(), "%s", car_error_.c_str());
    else if (car_modified()) ImGui::TextColored(color_warning(), "Alterações não salvas (já valem para as próximas simulações).");
    else ImGui::TextColored(color_ok(), "Dados válidos e salvos.");
    if (primary_button("Salvar", ImVec2(110, 0))) save_car(false);
    ImGui::SameLine();
    if (ImGui::Button("Salvar como...", ImVec2(130, 0))) save_car(true);
    ImGui::SameLine();
    ImGui::BeginDisabled(saved_json_.empty() || !car_modified());
    if (ImGui::Button("Desfazer alterações", ImVec2(170, 0))) {
        try {
            spec_ = fsae::io::parse_car_spec(saved_json_);
            ch = true;
        } catch (const std::exception&) {
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(!car_valid_);
    if (ImGui::Button("Rodar simulação (F5)", ImVec2(180, 0))) run_simulation(false);
    ImGui::EndDisabled();
    ImGui::SameLine(ImGui::GetWindowWidth() - 100);
    if (ImGui::Button("Fechar", ImVec2(80, 0))) show_editor_ = false;
    if (ImGui::IsKeyPressed(ImGuiKey_F5) && car_valid_ && ImGui::IsWindowFocused()) run_simulation(false);

    ImGui::End();
    if (ch) on_car_changed();
}

bool App::editor_general() {
    bool ch = false;
    auto& s = spec_;
    ImGui::Spacing();
    if (begin_form("geral")) {
        row_label("Nome do carro", nullptr);
        ch |= ImGui::InputText("##nome", &s.name);
        ch |= num("Massa do carro [kg]", s.car_mass_kg, "mass_only: carro sem piloto");
        ch |= num("Massa do piloto [kg]", s.driver_mass_kg, "pilot_mass");
        info("Massa total [kg]", "%.1f", s.car_mass_kg + s.driver_mass_kg);
        ch |= num("Gravidade [m/s²]", s.gravity_m_s2, "g");
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Passe o mouse sobre um campo para ver mais detalhes.");
    return ch;
}

bool App::editor_aero() {
    bool ch = false;
    auto& s = spec_;
    ImGui::Spacing();
    if (begin_form("aero")) {
        ch |= num("Área frontal [m²]", s.frontal_area_m2, "frontalArea");
        ch |= num("Densidade do ar [kg/m³]", s.air_density_kg_m3, "rho");
        ch |= num("Cl·A [m²]", s.cl_a_m2, "Coeficiente de sustentação × área (o programa calcula cl = ClA / A)");
        ch |= num("Cd·A [m²]", s.cd_a_m2, "Coeficiente de arrasto × área (cd = CdA / A)");
        ch |= num("Centro de pressão [% na traseira]", s.center_of_pressure_rear_percent, "cpaero");
        if (s.frontal_area_m2 > 0) {
            info("Cl (calculado)", "%.4f", s.cl_a_m2 / s.frontal_area_m2);
            info("Cd (calculado)", "%.4f", s.cd_a_m2 / s.frontal_area_m2);
            info("Eficiência L/D", "%.3f", s.cd_a_m2 != 0 ? s.cl_a_m2 / s.cd_a_m2 : 0.0);
        }
        ImGui::EndTable();
    }
    if (!car_valid_) return ch;
    // Downforce e arrasto por velocidade
    const fsae::Car car = fsae::Car::from_spec(s);
    std::vector<double> v(61), down(61), drag(61);
    for (int i = 0; i <= 60; ++i) {
        v[static_cast<std::size_t>(i)] = i * 2.5;  // km/h
        down[static_cast<std::size_t>(i)] = fsae::physics::downforce(car, v[static_cast<std::size_t>(i)] / 3.6);
        drag[static_cast<std::size_t>(i)] = fsae::physics::drag(car, v[static_cast<std::size_t>(i)] / 3.6);
    }
    ImGui::Spacing();
    if (ImPlot::BeginPlot("Forças aerodinâmicas", ImVec2(-1, -1))) {
        ImPlot::SetupAxes("Velocidade [km/h]", "Força [N]", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        ImPlot::SetNextLineStyle(run_palette(0), 2.5f);
        ImPlot::PlotLine("Downforce", v.data(), down.data(), 61);
        ImPlot::SetNextLineStyle(run_palette(1), 2.5f);
        ImPlot::PlotLine("Arrasto", v.data(), drag.data(), 61);
        ImPlot::EndPlot();
    }
    return ch;
}

bool App::editor_chassis() {
    bool ch = false;
    auto& s = spec_;
    ImGui::Spacing();
    if (begin_form("chassi")) {
        ch |= num("Entre-eixos [m]", s.wheelbase_m, "wheelbase");
        ch |= num("Raio do pneu [mm]", s.tire_radius_mm, "tire_radius (o programa converte para metros)");
        ch |= num("Distribuição de peso [% na traseira]", s.weight_dist_rear_percent, "weight_dist_rear_percent");
        ch |= num("Altura do CG [m]", s.cg_height_m, "cg_height");
        ch |= num("Bitola dianteira [m]", s.track_front_m, "track_front_m");
        ch |= num("Bitola traseira [m]", s.track_rear_m, "track_rear_m");
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Pneus");
    if (begin_form("pneus")) {
        ch |= num("Atrito longitudinal estático", s.mu_x_static, "mux_static");
        ch |= num("Atrito longitudinal cinético", s.mu_x_kinetic, "mux_kinetic: usado quando a roda patina");
        ch |= num("Atrito lateral", s.mu_y, "muy");
        ImGui::EndTable();
    }
    return ch;
}

bool App::editor_transmission() {
    bool ch = false;
    auto& s = spec_;
    ImGui::Spacing();
    ImGui::BeginChild("trans_esq", ImVec2(ImGui::GetContentRegionAvail().x * 0.48f, 0));
    if (begin_form("trans")) {
        ch |= num("Redução primária", s.primary_reduction);
        ch |= num("Redução final", s.final_drive);
        info("Redução total (primária × final)", "%.4f", s.primary_reduction * s.final_drive);
        ch |= num("Eficiência da transmissão", s.powertrain_efficiency, "powertrain_efficiency (0 a 1)");
        ch |= num("Tempo de troca [ms]", s.shift_time_ms, "shiftTime");
        ch |= num("RPM de largada (two-step)", s.launch_rpm, "launchRPM");
        ch |= num("RPM de redução", s.downshift_rpm, "downshiftRPM: reduz quando a rotação cai abaixo deste valor");
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
    ImGui::SeparatorText("Marchas");
    if (ImGui::Button("Importar planilha...")) import_from_file(ImportTarget::Gears);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("CSV, TXT ou Excel (.xlsx) com uma coluna de relações\ne, opcionalmente, rpm de subida.");
    ImGui::SameLine();
    if (ImGui::Button("Colar do Excel")) import_from_clipboard(ImportTarget::Gears);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Selecione as células no Excel, Ctrl+C, e clique aqui.");
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
    ImGui::EndChild();
    ImGui::SameLine();

    // Rotação × velocidade por marcha
    if (car_valid_) {
        const fsae::Car car = fsae::Car::from_spec(s);
        if (ImPlot::BeginPlot("Rotação × velocidade por marcha", ImVec2(-1, -1))) {
            ImPlot::SetupAxes("Velocidade [km/h]", "Rotação [rpm]", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
            for (int g = 1; g <= car.gear_count(); ++g) {
                double xs[2], ys[2];
                const double rpm1 = fsae::physics::engine_rpm(car, 1.0, g);
                ys[0] = car.engine.rpm_min();
                ys[1] = car.shift_rpm[static_cast<std::size_t>(g - 1)];
                for (int k = 0; k < 2; ++k) xs[k] = ys[k] / rpm1 * 3.6;
                ImPlot::SetNextLineStyle(run_palette(g - 1), 2.5f);
                ImPlot::PlotLine(("Marcha " + std::to_string(g)).c_str(), xs, ys, 2);
            }
            const double marks[] = {car.launch_rpm, car.downshift_rpm};
            ImPlot::SetNextLineStyle(color_muted(), 1.0f);
            ImPlot::PlotInfLines("Largada / redução", marks, 2, ImPlotInfLinesFlags_Horizontal);
            ImPlot::EndPlot();
        }
    }
    return ch;
}

bool App::editor_engine() {
    bool ch = false;
    auto& s = spec_;
    const float col_w = ImGui::GetContentRegionAvail().x * 0.40f;
    const float h = ImGui::GetContentRegionAvail().y;

    auto section = [&](const char* id, const char* title, ImportTarget target, const char* cy,
                       std::vector<double>& x, std::vector<double>& y, float height) {
        ImGui::PushID(id);
        ImGui::SeparatorText(title);
        if (ImGui::Button("Importar planilha...")) import_from_file(target);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("CSV, TXT ou Excel (.xlsx). Você escolhe as colunas e a unidade.");
        ImGui::SameLine();
        if (ImGui::Button("Colar do Excel")) import_from_clipboard(target);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Selecione as duas colunas no Excel, Ctrl+C, e clique aqui.");
        ch |= xy_table("tab", "rpm", cy, x, y, height);
        ImGui::PopID();
    };

    ImGui::BeginChild("motor_esq", ImVec2(col_w, 0));
    section("torque", "Curva de torque (pchip, como o dataEngine.m)", ImportTarget::Torque, "Torque [N.m]",
            s.engine_rpm, s.engine_torque_nm, h * 0.42f);
    section("bsfc", "Consumo específico (BSFC, linear)", ImportTarget::Bsfc, "BSFC [g/kWh]", s.bsfc_rpm,
            s.bsfc_g_kwh, h * 0.22f);
    ImGui::EndChild();
    ImGui::SameLine();

    if (car_valid_) {
        const fsae::Car car = fsae::Car::from_spec(s);
        constexpr int N = 300;
        std::vector<double> rpm(N), torque(N), power(N);
        for (int i = 0; i < N; ++i) {
            rpm[static_cast<std::size_t>(i)] = car.engine.rpm_min() + (car.engine.rpm_max() - car.engine.rpm_min()) * i / (N - 1);
            torque[static_cast<std::size_t>(i)] = car.engine.torque(rpm[static_cast<std::size_t>(i)]);
            power[static_cast<std::size_t>(i)] = torque[static_cast<std::size_t>(i)] * rpm[static_cast<std::size_t>(i)] / 9549.0;
        }
        if (ImPlot::BeginSubplots("##motor_prev", 2, 1, ImVec2(-1, -1), ImPlotSubplotFlags_LinkAllX)) {
            if (ImPlot::BeginPlot("Torque")) {
                ImPlot::SetupAxes(nullptr, "Torque [N.m]", ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_NoTickLabels,
                                  ImPlotAxisFlags_AutoFit);
                ImPlot::SetNextLineStyle(run_palette(0), 2.5f);
                ImPlot::PlotLine("Interpolado", rpm.data(), torque.data(), N);
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4, run_palette(0));
                ImPlot::PlotScatter("Pontos", s.engine_rpm.data(), s.engine_torque_nm.data(),
                                    static_cast<int>(std::min(s.engine_rpm.size(), s.engine_torque_nm.size())));
                ImPlot::EndPlot();
            }
            if (ImPlot::BeginPlot("Potência")) {
                ImPlot::SetupAxes("Rotação [rpm]", "Potência [kW]", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                ImPlot::SetNextLineStyle(run_palette(1), 2.5f);
                ImPlot::PlotLine("Potência", rpm.data(), power.data(), N);
                ImPlot::EndPlot();
            }
            ImPlot::EndSubplots();
        }
    } else {
        ImGui::TextColored(color_error(), "Corrija os dados para ver as curvas:\n%s", car_error_.c_str());
    }
    return ch;
}

bool App::editor_brakes() {
    bool ch = false;
    auto& s = spec_;
    ImGui::Spacing();
    ImGui::SeparatorText("Disco de freio dianteiro (modelo térmico)");
    if (begin_form("freio")) {
        ch |= num("Massa do disco [kg]", s.brake_disc_mass_kg, "brake_mass");
        ch |= num("Diâmetro do disco [m]", s.brake_disc_diameter_m, "brake_diameter");
        ch |= num("Área exposta do disco [m²]", s.brake_disc_surface_area_m2, "brake_surface_area");
        ch |= num("Condutividade do disco [W/m·K]", s.brake_disc_thermal_conductivity_w_mk, "brake_thermalcond");
        ch |= num("Emissividade do disco", s.brake_disc_emissivity, "brake_emissivity");
        ch |= num("Número de bobbins", s.brake_bobbin_count, "Fixações do disco (área de condução para o cubo)");
        ch |= num("Diâmetro do bobbin [m]", s.brake_bobbin_diameter_m);
        ch |= num("Distância de condução [m]", s.brake_conduction_distance_m, "brake_cond_dist");
        ImGui::EndTable();
    }
    ImGui::SeparatorText("Ar");
    if (begin_form("ar")) {
        ch |= num("Condutividade do ar [W/m·K]", s.air_thermal_conductivity_w_mk, "air_k");
        ch |= num("Viscosidade do ar [Pa·s]", s.air_dynamic_viscosity_pa_s, "air_mu", "%.4g");
        ImGui::EndTable();
    }
    ImGui::Spacing();
    ImGui::TextDisabled("A força de frenagem é fixa no modelo do MATLAB (3000 N·m no raio do pneu).");
    return ch;
}

void App::car_summary() {
    if (!car_valid_) {
        ImGui::TextColored(color_error(), "Carro inválido.");
        return;
    }
    const fsae::Car car = fsae::Car::from_spec(spec_);
    const auto& rpm = car.engine.torque_rpm();
    const auto& tq = car.engine.torque_nm();
    std::size_t it = 0, ip = 0;
    for (std::size_t i = 0; i < rpm.size(); ++i) {
        if (tq[i] > tq[it]) it = i;
        if (tq[i] * rpm[i] > tq[ip] * rpm[ip]) ip = i;
    }
    if (ImGui::BeginTable("resumo", 2, ImGuiTableFlags_RowBg)) {
        auto line = [](const char* k, const std::string& v) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(k);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(v.c_str());
        };
        char b[96];
        std::snprintf(b, sizeof b, "%.1f kg", car.mass);
        line("Massa total", b);
        std::snprintf(b, sizeof b, "%.2f / %.2f m²", spec_.cl_a_m2, spec_.cd_a_m2);
        line("Cl·A / Cd·A", b);
        std::snprintf(b, sizeof b, "%.0f N", fsae::physics::downforce(car, 100 / 3.6));
        line("Downforce a 100 km/h", b);
        std::snprintf(b, sizeof b, "%.2f / %.2f", car.mux_static, car.muy);
        line("Atrito long. / lateral", b);
        std::snprintf(b, sizeof b, "%d, total %.3f", car.gear_count(), car.overall_reduction);
        line("Marchas, redução", b);
        std::snprintf(b, sizeof b, "%.1f N.m a %.0f rpm", tq[it], rpm[it]);
        line("Torque máximo", b);
        std::snprintf(b, sizeof b, "%.1f kW a %.0f rpm", tq[ip] * rpm[ip] / 9549.0, rpm[ip]);
        line("Potência máxima", b);
        ImGui::EndTable();
    }
}

}  // namespace fsae::gui
