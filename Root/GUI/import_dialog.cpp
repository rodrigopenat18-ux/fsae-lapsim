#include "import_dialog.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <numeric>

#include <imgui.h>
#include <implot.h>

#include "platform.hpp"
#include "theme.hpp"

namespace fsae::gui {

namespace {

constexpr const char* kPopup = "Importar dados";

struct Unit {
    const char* name;
    double factor;  ///< multiplica o valor importado para chegar à unidade do simulador
};

const Unit kTorqueUnits[] = {{"N.m", 1.0}, {"kgf.m", 9.80665}, {"lbf.ft", 1.3558179483314}};
const Unit kBsfcUnits[] = {{"g/kWh", 1.0}, {"lb/(hp.h)", 608.27738}};

const char* title(ImportTarget t) {
    switch (t) {
        case ImportTarget::Torque: return "Curva de torque";
        case ImportTarget::Bsfc: return "Consumo específico (BSFC)";
        case ImportTarget::Gears: break;
    }
    return "Relações de marcha";
}

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

int find_column(const fsae::io::DataTable& t, std::initializer_list<const char*> keys, int avoid = -1) {
    for (const char* k : keys) {
        for (std::size_t c = 0; c < t.cols(); ++c) {
            if (static_cast<int>(c) != avoid && lower(t.columns[c]).find(k) != std::string::npos) {
                return static_cast<int>(c);
            }
        }
    }
    return -1;
}

bool column_combo(const char* label, int& col, const fsae::io::DataTable& t, bool allow_none) {
    bool changed = false;
    const std::string preview = col < 0 ? "(manter os valores atuais)" : t.columns[static_cast<std::size_t>(col)];
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        if (allow_none && ImGui::Selectable("(manter os valores atuais)", col < 0)) {
            col = -1;
            changed = true;
        }
        for (std::size_t c = 0; c < t.cols(); ++c) {
            if (ImGui::Selectable(t.columns[c].c_str(), static_cast<int>(c) == col)) {
                col = static_cast<int>(c);
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

}  // namespace

void ImportDialog::open_file(ImportTarget target, const std::filesystem::path& path) {
    target_ = target;
    path_ = path;
    text_.clear();
    sheets_.clear();
    sheet_ = 0;
    std::string ext = lower(path.extension().string());
    if (ext == ".xlsx" || ext == ".xlsm") {
        try {
            sheets_ = fsae::io::xlsx_sheet_names(path);
        } catch (const std::exception&) {
        }
    }
    load();
    pending_open_ = true;
}

void ImportDialog::open_text(ImportTarget target, const std::string& text) {
    target_ = target;
    path_.clear();
    text_ = text;
    sheets_.clear();
    sheet_ = 0;
    load();
    pending_open_ = true;
}

void ImportDialog::load() {
    load_error_.clear();
    table_ = {};
    try {
        table_ = path_.empty() ? fsae::io::parse_delimited_table(text_)
                               : fsae::io::read_table_file(path_, static_cast<std::size_t>(sheet_));
        guess_columns();
    } catch (const std::exception& e) {
        load_error_ = e.what();
    }
}

void ImportDialog::guess_columns() {
    const int n = static_cast<int>(table_.cols());
    unit_y_ = 0;
    factor_x_ = 1.0;
    col_z_ = -1;
    if (target_ == ImportTarget::Gears) {
        col_y_ = find_column(table_, {"rela", "ratio", "reduc"});
        if (col_y_ < 0) col_y_ = n > 1 ? 1 : 0;
        col_z_ = find_column(table_, {"subida", "shift", "troca", "upshift"}, col_y_);
        col_x_ = -1;
        return;
    }
    col_x_ = find_column(table_, {"rpm", "rot", "speed", "veloc"});
    if (col_x_ < 0) col_x_ = 0;
    const auto keys = target_ == ImportTarget::Torque ? std::initializer_list<const char*>{"torq", "tq", "n.m", "nm"}
                                                      : std::initializer_list<const char*>{"bsfc", "consumo", "g/kwh"};
    col_y_ = find_column(table_, keys, col_x_);
    if (col_y_ < 0) col_y_ = std::min(n - 1, col_x_ == 0 ? 1 : 0);
}

bool ImportDialog::collect(std::vector<double>& x, std::vector<double>& y, std::vector<double>& z,
                           std::string& error) const {
    x.clear();
    y.clear();
    z.clear();
    const std::size_t rows = table_.rows();
    if (target_ == ImportTarget::Gears) {
        if (col_y_ < 0) {
            error = "escolha a coluna das relações";
            return false;
        }
        for (std::size_t i = 0; i < rows; ++i) {
            const double r = table_.values[static_cast<std::size_t>(col_y_)][i];
            if (std::isnan(r)) continue;
            y.push_back(r);
            z.push_back(col_z_ >= 0 ? table_.values[static_cast<std::size_t>(col_z_)][i] : std::nan(""));
        }
        if (y.empty() || y.size() > 10) {
            error = "esperadas de 1 a 10 marchas (encontradas " + std::to_string(y.size()) + ")";
            return false;
        }
        for (double r : y) {
            if (!(r > 0)) {
                error = "relações precisam ser positivas";
                return false;
            }
        }
        if (col_z_ >= 0) {
            for (double v : z) {
                if (!(v > 0)) {
                    error = "há marchas sem rpm de subida válido";
                    return false;
                }
            }
        }
        return true;
    }

    const double fy = target_ == ImportTarget::Torque ? kTorqueUnits[unit_y_].factor : kBsfcUnits[unit_y_].factor;
    std::vector<std::pair<double, double>> pts;
    for (std::size_t i = 0; i < rows; ++i) {
        const double a = table_.values[static_cast<std::size_t>(col_x_)][i];
        const double b = table_.values[static_cast<std::size_t>(col_y_)][i];
        if (std::isnan(a) || std::isnan(b)) continue;
        pts.emplace_back(a * factor_x_, b * fy);
    }
    std::sort(pts.begin(), pts.end());
    for (std::size_t i = 1; i < pts.size(); ++i) {
        if (pts[i].first == pts[i - 1].first) {
            error = "rpm repetido (" + std::to_string(pts[i].first) + "): cada rotação deve aparecer uma vez";
            return false;
        }
    }
    if (pts.size() < 2) {
        error = "são necessários pelo menos 2 pontos válidos";
        return false;
    }
    for (const auto& [a, b] : pts) {
        x.push_back(a);
        y.push_back(b);
    }
    return true;
}

bool ImportDialog::draw(fsae::CarSpec& spec) {
    if (pending_open_) {
        ImGui::OpenPopup(kPopup);
        pending_open_ = false;
    }
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowSize(ImVec2(std::min(1000.0f, vp->WorkSize.x * 0.9f), std::min(720.0f, vp->WorkSize.y * 0.9f)),
                             ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f),
                            ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    bool applied = false;
    if (!ImGui::BeginPopupModal(kPopup, nullptr, ImGuiWindowFlags_NoSavedSettings)) return false;

    ImGui::Text("%s", title(target_));
    ImGui::TextDisabled("Origem: %s", path_.empty() ? "texto colado" : to_utf8(path_.filename()).c_str());

    if (!sheets_.empty()) {
        ImGui::SetNextItemWidth(260);
        if (ImGui::BeginCombo("Planilha", sheets_[static_cast<std::size_t>(sheet_)].c_str())) {
            for (int i = 0; i < static_cast<int>(sheets_.size()); ++i) {
                if (ImGui::Selectable(sheets_[static_cast<std::size_t>(i)].c_str(), i == sheet_)) {
                    sheet_ = i;
                    load();
                }
            }
            ImGui::EndCombo();
        }
    }

    std::vector<double> x, y, z;
    std::string error;
    bool valid = false;
    if (!load_error_.empty()) {
        ImGui::TextColored(color_error(), "Não foi possível ler: %s", load_error_.c_str());
    } else {
        ImGui::TextDisabled("%zu colunas, %zu linhas de dados (a partir da linha %zu).", table_.cols(),
                            table_.rows(), table_.first_data_row + 1);
        if (target_ == ImportTarget::Gears) {
            column_combo("Relação de cada marcha", col_y_, table_, false);
            column_combo("RPM de subida", col_z_, table_, true);
        } else {
            column_combo("Rotação [rpm]", col_x_, table_, false);
            column_combo(target_ == ImportTarget::Torque ? "Torque" : "BSFC", col_y_, table_, false);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(120);
            const Unit* units = target_ == ImportTarget::Torque ? kTorqueUnits : kBsfcUnits;
            const int nunits = target_ == ImportTarget::Torque ? IM_ARRAYSIZE(kTorqueUnits) : IM_ARRAYSIZE(kBsfcUnits);
            if (ImGui::BeginCombo("Unidade", units[unit_y_].name)) {
                for (int i = 0; i < nunits; ++i) {
                    if (ImGui::Selectable(units[i].name, i == unit_y_)) unit_y_ = i;
                }
                ImGui::EndCombo();
            }
            ImGui::SetNextItemWidth(120);
            ImGui::InputDouble("Multiplicar rotação por", &factor_x_, 0, 0, "%.6g");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Use 1000 se a planilha estiver em mil rpm, por exemplo.");
        }
        valid = collect(x, y, z, error);
        if (!valid) ImGui::TextColored(color_error(), "%s", error.c_str());
        else ImGui::TextColored(color_ok(), "%zu pontos prontos para importar.", y.size());

        // Prévia: tabela + gráfico
        const float h = ImGui::GetContentRegionAvail().y - ImGui::GetFrameHeightWithSpacing() * 1.6f;
        if (ImGui::BeginTable("previa", static_cast<int>(std::min<std::size_t>(table_.cols(), 12)),
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_ScrollX,
                              ImVec2(ImGui::GetContentRegionAvail().x * 0.45f, h))) {
            ImGui::TableSetupScrollFreeze(0, 1);
            for (std::size_t c = 0; c < std::min<std::size_t>(table_.cols(), 12); ++c) {
                ImGui::TableSetupColumn(table_.columns[c].c_str());
            }
            ImGui::TableHeadersRow();
            for (std::size_t r = 0; r < table_.rows(); ++r) {
                ImGui::TableNextRow();
                for (std::size_t c = 0; c < std::min<std::size_t>(table_.cols(), 12); ++c) {
                    ImGui::TableSetColumnIndex(static_cast<int>(c));
                    const double v = table_.values[c][r];
                    const bool used = static_cast<int>(c) == col_x_ || static_cast<int>(c) == col_y_ ||
                                      static_cast<int>(c) == col_z_;
                    if (std::isnan(v)) ImGui::TextDisabled("-");
                    else if (used) ImGui::Text("%.6g", v);
                    else ImGui::TextDisabled("%.6g", v);
                }
            }
            ImGui::EndTable();
        }
        ImGui::SameLine();
        if (ImPlot::BeginPlot("##previa", ImVec2(-1, h))) {
            if (target_ == ImportTarget::Gears) {
                ImPlot::SetupAxes("Marcha", "Relação", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                std::vector<double> g(spec.gear_ratios.size()), gi(y.size());
                std::iota(g.begin(), g.end(), 1.0);
                std::iota(gi.begin(), gi.end(), 1.0);
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5);
                ImPlot::SetNextLineStyle(color_muted());
                ImPlot::PlotLine("Atual", g.data(), spec.gear_ratios.data(), static_cast<int>(g.size()));
                if (valid) {
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 5);
                    ImPlot::SetNextLineStyle(run_palette(0), 2.5f);
                    ImPlot::PlotLine("Importado", gi.data(), y.data(), static_cast<int>(gi.size()));
                }
            } else {
                const bool tq = target_ == ImportTarget::Torque;
                ImPlot::SetupAxes("Rotação [rpm]", tq ? "Torque [N.m]" : "BSFC [g/kWh]", ImPlotAxisFlags_AutoFit,
                                  ImPlotAxisFlags_AutoFit);
                const auto& cx = tq ? spec.engine_rpm : spec.bsfc_rpm;
                const auto& cy = tq ? spec.engine_torque_nm : spec.bsfc_g_kwh;
                ImPlot::SetNextLineStyle(color_muted());
                ImPlot::PlotLine("Atual", cx.data(), cy.data(), static_cast<int>(std::min(cx.size(), cy.size())));
                if (valid) {
                    ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 3);
                    ImPlot::SetNextLineStyle(run_palette(0), 2.5f);
                    ImPlot::PlotLine("Importado", x.data(), y.data(), static_cast<int>(x.size()));
                }
            }
            ImPlot::EndPlot();
        }
    }

    ImGui::BeginDisabled(!valid);
    if (primary_button("Importar", ImVec2(140, 0))) {
        switch (target_) {
            case ImportTarget::Torque:
                spec.engine_rpm = x;
                spec.engine_torque_nm = y;
                break;
            case ImportTarget::Bsfc:
                spec.bsfc_rpm = x;
                spec.bsfc_g_kwh = y;
                break;
            case ImportTarget::Gears: {
                const double fallback = spec.upshift_rpm.empty() ? 12000.0 : spec.upshift_rpm.back();
                std::vector<double> up(y.size(), fallback);
                for (std::size_t i = 0; i < y.size(); ++i) {
                    if (col_z_ >= 0) up[i] = z[i];
                    else if (i < spec.upshift_rpm.size()) up[i] = spec.upshift_rpm[i];
                }
                spec.gear_ratios = y;
                spec.upshift_rpm = up;
                break;
            }
        }
        applied = true;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancelar", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return applied;
}

}  // namespace fsae::gui
