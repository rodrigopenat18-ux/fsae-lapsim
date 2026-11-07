// Abas de gráficos da interface.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <imgui.h>
#include <implot.h>
#include <misc/cpp/imgui_stdlib.h>

#include "app.hpp"
#include "fsae/control/gearbox.hpp"
#include "fsae/physics/powertrain.hpp"
#include "theme.hpp"

namespace fsae::gui {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// Turbo (Google): aproximação polinomial publicada por Anton Mikhailov.
ImVec4 turbo(float t) {
    const float t2 = t * t, t3 = t2 * t, t4 = t3 * t, t5 = t4 * t;
    auto clamp01 = [](float v) { return std::min(1.0f, std::max(0.0f, v)); };
    return ImVec4(
        clamp01(0.13572138f + 4.61539260f * t - 42.66032258f * t2 + 132.13108234f * t3 - 152.94239396f * t4 + 59.28637943f * t5),
        clamp01(0.09140261f + 2.19418839f * t + 4.84296658f * t2 - 14.18503333f * t3 + 4.27729857f * t4 + 2.82956604f * t5),
        clamp01(0.10667330f + 12.64194608f * t - 60.58204836f * t2 + 110.36276771f * t3 - 89.90310912f * t4 + 27.34824973f * t5),
        1.0f);
}

/// Cria (uma vez) um mapa de cores a partir de amostras.
ImPlotColormap custom_colormap(const char* name, const std::vector<ImVec4>& c) {
    ImPlotColormap id = ImPlot::GetColormapIndex(name);
    if (id < 0) id = ImPlot::AddColormap(name, c.data(), static_cast<int>(c.size()), false);
    return id;
}

struct ColormapOption {
    const char* name;
    ImPlotColormap builtin;  ///< -1 = personalizado
};

const ColormapOption kColormaps[] = {
    {"Viridis", ImPlotColormap_Viridis},  {"Turbo", -1},
    {"Plasma", ImPlotColormap_Plasma},    {"Jet", ImPlotColormap_Jet},
    {"Hot", ImPlotColormap_Hot},          {"Cool", ImPlotColormap_Cool},
    {"Vermelho-Azul (RdBu)", ImPlotColormap_RdBu},
    {"Espectral", ImPlotColormap_Spectral},
    {"Marrom-Verde (BrBG)", ImPlotColormap_BrBG},
    {"Rosa-Verde (PiYG)", ImPlotColormap_PiYG},
    {"Twilight", ImPlotColormap_Twilight},
    {"Rosa", ImPlotColormap_Pink},
    {"Cinzas", ImPlotColormap_Greys},
    {"Azul (um tom)", -1},
};

ImPlotColormap base_colormap(int i) {
    const auto& o = kColormaps[static_cast<std::size_t>(i)];
    if (o.builtin >= 0) return o.builtin;
    std::vector<ImVec4> c;
    if (std::string(o.name) == "Turbo") {
        for (int k = 0; k < 64; ++k) c.push_back(turbo(static_cast<float>(k) / 63.0f));
    } else {  // azul de um tom só
        c = {ImVec4(0.86f, 0.92f, 0.98f, 1), ImVec4(0.62f, 0.78f, 0.94f, 1), ImVec4(0.35f, 0.60f, 0.88f, 1),
             ImVec4(0.16f, 0.42f, 0.76f, 1), ImVec4(0.08f, 0.26f, 0.52f, 1)};
    }
    return custom_colormap(o.name, c);
}

/// Mapa de cores escolhido, invertido se pedido (cópia invertida criada sob demanda).
ImPlotColormap pick_colormap(int i, bool invert) {
    const ImPlotColormap base = base_colormap(i);
    if (!invert) return base;
    const std::string name = std::string(kColormaps[static_cast<std::size_t>(i)].name) + " (invertido)";
    ImPlotColormap id = ImPlot::GetColormapIndex(name.c_str());
    if (id >= 0) return id;
    std::vector<ImVec4> c;
    const int n = ImPlot::GetColormapSize(base);
    for (int k = n - 1; k >= 0; --k) c.push_back(ImPlot::GetColormapColor(k, base));
    return custom_colormap(name.c_str(), c);
}

}  // namespace

int colormap_count() { return IM_ARRAYSIZE(kColormaps); }
const char* colormap_name(int i) {
    return kColormaps[static_cast<std::size_t>(std::clamp(i, 0, colormap_count() - 1))].name;
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

namespace {

std::string item_id(const Run& r) { return r.label + "##" + std::to_string(r.id); }

std::string axis_label(int ch) {
    const auto& c = channels()[static_cast<std::size_t>(ch)];
    return std::string(c.label) + (c.unit[0] ? std::string(" [") + c.unit + "]" : "");
}

bool channel_combo(const char* id, int& ch) {
    bool changed = false;
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo(id, channels()[static_cast<std::size_t>(ch)].label)) {
        for (int i = 0; i < static_cast<int>(channels().size()); ++i) {
            if (ImGui::Selectable(axis_label(i).c_str(), i == ch)) {
                ch = i;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    return changed;
}

bool no_runs(const std::vector<Run>& runs) {
    if (!runs.empty()) return false;
    ImGui::TextDisabled("Rode uma simulação (painel da esquerda) para ver os gráficos.");
    return true;
}

/// Índice do ponto do traçado para cada distância (comprimento de arco acumulado).
std::vector<std::size_t> track_index(const fsae::track::TrackGeometry& g,
                                     const std::vector<double>& dist) {
    std::vector<double> s(g.size(), 0.0);
    for (std::size_t i = 1; i < g.size(); ++i) {
        s[i] = s[i - 1] + std::hypot(g.x[i] - g.x[i - 1], g.y[i] - g.y[i - 1]);
    }
    std::vector<std::size_t> idx(dist.size(), 0);
    for (std::size_t i = 0; i < dist.size(); ++i) {
        const auto it = std::lower_bound(s.begin(), s.end(), dist[i]);
        idx[i] = std::min<std::size_t>(static_cast<std::size_t>(it - s.begin()), g.size() - 1);
    }
    return idx;
}

/// Tempo interpolado na distância `d` (distância não decrescente).
double time_at(const Series& s, double d) {
    if (s.x.empty()) return kNaN;
    if (d <= s.x.front()) return s.y.front();
    if (d >= s.x.back()) return kNaN;
    const auto it = std::upper_bound(s.x.begin(), s.x.end(), d);
    const std::size_t k = static_cast<std::size_t>(it - s.x.begin());
    const double x0 = s.x[k - 1], x1 = s.x[k];
    if (x1 == x0) return s.y[k];
    return s.y[k - 1] + (s.y[k] - s.y[k - 1]) * (d - x0) / (x1 - x0);
}

}  // namespace

// Seletor de volta comum às abas (-1 última, 0 todas, k volta k)
static bool lap_combo(int& lap, int max_laps) {
    ImGui::SetNextItemWidth(150);
    std::string preview = lap < 0 ? "Última volta" : lap == 0 ? "Todas as voltas" : "Volta " + std::to_string(lap);
    bool changed = false;
    if (ImGui::BeginCombo("Volta", preview.c_str())) {
        if (ImGui::Selectable("Última volta", lap < 0)) { lap = -1; changed = true; }
        if (ImGui::Selectable("Todas as voltas", lap == 0)) { lap = 0; changed = true; }
        for (int k = 1; k <= max_laps; ++k) {
            if (ImGui::Selectable(("Volta " + std::to_string(k)).c_str(), lap == k)) {
                lap = k;
                changed = true;
            }
        }
        ImGui::EndCombo();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Como o runSimulation do MATLAB, o padrão é a última volta (a lançada).\n"
                          "Tempo e distância de cada volta começam em zero.");
    }
    return changed;
}

// ---------------------------------------------------------------------------
void App::tab_telemetry() {
    if (no_runs(runs_)) return;
    if (lap_combo(plot_lap_, max_laps())) ++view_gen_;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    int xd = x_distance_ ? 1 : 0;
    const char* xs[] = {"Tempo", "Distância"};
    if (ImGui::Combo("Eixo X", &xd, xs, 2)) {
        x_distance_ = xd == 1;
        ++view_gen_;
    }

    ImGui::BeginChild("canais", ImVec2(230, 0), ImGuiChildFlags_Border);
    ImGui::TextDisabled("Canais");
    const auto& ch = channels();
    for (std::size_t i = 2; i < ch.size(); ++i) {
        bool v = tel_selected_[i];
        if (ImGui::Checkbox(ch[i].label, &v)) {
            tel_selected_[i] = v;
            ++view_gen_;
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    std::vector<int> sel;
    for (std::size_t i = 2; i < ch.size(); ++i) {
        if (tel_selected_[i]) sel.push_back(static_cast<int>(i));
    }
    if (sel.empty()) {
        ImGui::TextDisabled("Escolha pelo menos um canal.");
        return;
    }
    const bool fit = needs_fit(0);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (ImPlot::BeginSubplots("##telemetria", static_cast<int>(sel.size()), 1, avail,
                              ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_ShareItems)) {
        for (std::size_t p = 0; p < sel.size(); ++p) {
            const int c = sel[p];
            if (fit) ImPlot::SetNextAxesToFit();
            if (ImPlot::BeginPlot(("##p" + std::to_string(c)).c_str())) {
                const bool last = p + 1 == sel.size();
                ImPlot::SetupAxes(last ? (x_distance_ ? "Distância [m]" : "Tempo [s]") : nullptr,
                                  axis_label(c).c_str(),
                                  last ? 0 : ImPlotAxisFlags_NoTickLabels,
                                  ImPlotAxisFlags_AutoFit | ImPlotAxisFlags_RangeFit);
                for (const auto& r : runs_) {
                    if (!r.visible) continue;
                    const Series s = series(r, c, plot_lap_, x_distance_);
                    if (s.x.empty()) continue;
                    ImPlot::SetNextLineStyle(run_color(r), 2.0f);
                    const std::string id = item_id(r);
                    if (ch[static_cast<std::size_t>(c)].stairs) {
                        ImPlot::PlotStairs(id.c_str(), s.x.data(), s.y.data(), static_cast<int>(s.x.size()));
                    } else {
                        ImPlot::PlotLine(id.c_str(), s.x.data(), s.y.data(), static_cast<int>(s.x.size()));
                    }
                }
                ImPlot::EndPlot();
            }
        }
        ImPlot::EndSubplots();
    }
}

// ---------------------------------------------------------------------------
void App::tab_track_map() {
    if (no_runs(runs_)) return;
    if (map_run_ >= static_cast<int>(runs_.size())) map_run_ = -1;
    const Run& r = map_run_ < 0 ? runs_.back() : runs_[static_cast<std::size_t>(map_run_)];

    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Execução", r.label.c_str())) {
        for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
            if (ImGui::Selectable(runs_[static_cast<std::size_t>(i)].label.c_str(), &runs_[static_cast<std::size_t>(i)] == &r)) {
                map_run_ = i == static_cast<int>(runs_.size()) - 1 ? -1 : i;
                ++view_gen_;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (channel_combo("Cor", map_channel_)) ++view_gen_;
    ImGui::SameLine();
    if (lap_combo(plot_lap_, max_laps())) ++view_gen_;

    // Segunda linha: aparência do mapa
    ImGui::SetNextItemWidth(220);
    if (ImGui::BeginCombo("Mapa de cores", colormap_name(map_cmap_))) {
        for (int i = 0; i < colormap_count(); ++i) {
            ImGui::PushID(i);
            // Amostra do mapa de cores ao lado do nome
            const ImPlotColormap cm = pick_colormap(i, map_invert_);
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float w = 70, h = ImGui::GetTextLineHeight();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            for (int k = 0; k < 14; ++k) {
                const ImU32 col = ImGui::GetColorU32(ImPlot::SampleColormap(static_cast<float>(k) / 13.0f, cm));
                const float k0 = static_cast<float>(k), k1 = static_cast<float>(k + 1);
                dl->AddRectFilled(ImVec2(p.x + w * k0 / 14, p.y + 2), ImVec2(p.x + w * k1 / 14, p.y + h - 1), col);
            }
            ImGui::Dummy(ImVec2(w + 6, h));
            ImGui::SameLine();
            if (ImGui::Selectable(colormap_name(i), i == map_cmap_)) map_cmap_ = i;
            ImGui::PopID();
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Inverter", &map_invert_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140);
    ImGui::SliderFloat("Espessura", &map_width_, 2.0f, 20.0f, "%.0f px");

    const Series vals = series(r, map_channel_, plot_lap_, true);
    if (vals.x.empty()) {
        ImGui::TextDisabled("Esta execução não tem essa volta.");
        return;
    }
    // Distância dentro da volta (para "todas as voltas", rebate em cada volta)
    std::vector<double> d = vals.x;
    const double L = r.result.track_length;
    if (plot_lap_ == 0 && L > 0) {
        for (double& v : d) v = std::fmod(v, L);
    }
    const auto idx = track_index(r.geometry, d);
    double vmin = *std::min_element(vals.y.begin(), vals.y.end());
    double vmax = *std::max_element(vals.y.begin(), vals.y.end());
    if (!(vmax > vmin)) vmax = vmin + 1;

    const ImPlotColormap cmap = pick_colormap(map_cmap_, map_invert_);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float scale_w = 90;
    if (needs_fit(1)) ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("##mapa", ImVec2(avail.x - scale_w, avail.y), ImPlotFlags_Equal | ImPlotFlags_NoLegend)) {
        ImPlot::SetupAxes(nullptr, nullptr, ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_NoTickLabels);
        const auto& g = r.geometry;
        ImPlot::SetNextLineStyle(color_track(), map_width_ + 5.0f);
        ImPlot::PlotLine("pista", g.x.data(), g.y.data(), static_cast<int>(g.size()));

        ImPlot::PushPlotClipRect();
        ImDrawList* dl = ImPlot::GetPlotDrawList();
        for (std::size_t i = 1; i < idx.size(); ++i) {
            const double t = (vals.y[i] - vmin) / (vmax - vmin);
            const ImU32 col = ImGui::GetColorU32(ImPlot::SampleColormap(static_cast<float>(t), cmap));
            const ImVec2 a = ImPlot::PlotToPixels(g.x[idx[i - 1]], g.y[idx[i - 1]]);
            const ImVec2 b = ImPlot::PlotToPixels(g.x[idx[i]], g.y[idx[i]]);
            dl->AddLine(a, b, col, map_width_);
        }
        // Largada
        const ImVec2 start = ImPlot::PlotToPixels(g.x.front(), g.y.front());
        dl->AddCircleFilled(start, map_width_ * 0.5f + 4, ImGui::GetColorU32(ImGuiCol_Text));
        dl->AddCircleFilled(start, map_width_ * 0.5f + 2, ImGui::GetColorU32(ImPlot::GetStyle().Colors[ImPlotCol_PlotBg]));
        ImPlot::PopPlotClipRect();
        ImPlot::PlotText("largada", g.x.front(), g.y.front(), ImVec2(0, -16));

        if (ImPlot::IsPlotHovered()) {
            const ImPlotPoint m = ImPlot::GetPlotMousePos();
            std::size_t best = 0;
            double best_d = 1e300;
            for (std::size_t i = 0; i < idx.size(); ++i) {
                const double dx = g.x[idx[i]] - m.x, dy = g.y[idx[i]] - m.y;
                if (dx * dx + dy * dy < best_d) { best_d = dx * dx + dy * dy; best = i; }
            }
            const auto& c = channels()[static_cast<std::size_t>(map_channel_)];
            ImGui::BeginTooltip();
            ImGui::Text("%s: %.4g %s", c.label, vals.y[best], c.unit);
            ImGui::Text("Distância: %.1f m", vals.x[best]);
            const Series tt = series(r, 0, plot_lap_, true);
            if (best < tt.y.size()) ImGui::Text("Tempo: %.3f s", tt.y[best]);
            ImGui::EndTooltip();
        }
        ImPlot::EndPlot();
    }
    ImGui::SameLine();
    ImPlot::ColormapScale(axis_label(map_channel_).c_str(), vmin, vmax, ImVec2(scale_w - 10, avail.y), "%g",
                          0, cmap);
}

// ---------------------------------------------------------------------------
void App::tab_gg() {
    if (no_runs(runs_)) return;
    if (lap_combo(plot_lap_, max_laps())) ++view_gen_;
    const int lat = channel_index("lateral_accel");
    const int lon = channel_index("acceleration");
    if (needs_fit(2)) ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("##gg", ImGui::GetContentRegionAvail(), ImPlotFlags_Equal)) {
        ImPlot::SetupAxes("Aceleração lateral [g]", "Aceleração longitudinal [g]");
        for (const auto& r : runs_) {
            if (!r.visible) continue;
            const Series s = series_xy(r, lat, lon, plot_lap_);
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 3.0f, run_color(r), 0.0f);
            ImPlot::PlotScatter(item_id(r).c_str(), s.x.data(), s.y.data(), static_cast<int>(s.x.size()));
        }
        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
void App::tab_xy() {
    if (no_runs(runs_)) return;
    if (channel_combo("Eixo X##xy", xy_x_)) ++view_gen_;
    ImGui::SameLine();
    if (channel_combo("Eixo Y##xy", xy_y_)) ++view_gen_;
    ImGui::SameLine();
    if (lap_combo(plot_lap_, max_laps())) ++view_gen_;
    ImGui::SameLine();
    ImGui::Checkbox("Ligar pontos", &xy_lines_);
    if (needs_fit(3)) ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("##xy", ImGui::GetContentRegionAvail())) {
        ImPlot::SetupAxes(axis_label(xy_x_).c_str(), axis_label(xy_y_).c_str());
        for (const auto& r : runs_) {
            if (!r.visible) continue;
            const Series s = series_xy(r, xy_x_, xy_y_, plot_lap_);
            const std::string id = item_id(r);
            if (xy_lines_) {
                ImPlot::SetNextLineStyle(run_color(r), 1.5f);
                ImPlot::PlotLine(id.c_str(), s.x.data(), s.y.data(), static_cast<int>(s.x.size()));
            } else {
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 3.0f, run_color(r), 0.0f);
                ImPlot::PlotScatter(id.c_str(), s.x.data(), s.y.data(), static_cast<int>(s.x.size()));
            }
        }
        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
void App::tab_engine() {
    if (!car_valid_) {
        ImGui::TextColored(color_error(), "Carro inválido: %s", car_error_.c_str());
        return;
    }
    const fsae::Car car = fsae::Car::from_spec(spec_);
    ImGui::TextDisabled("Curvas do carro em edição. Fora da faixa medida (%.0f a %.0f rpm) a interpolação pchip"
                        " extrapola sem limite (KNOWN_ISSUES, item 6), por isso não é desenhada.",
                        car.engine.rpm_min(), car.engine.rpm_max());

    constexpr int N = 400;
    std::vector<double> rpm(N), torque(N), power(N);
    for (int i = 0; i < N; ++i) {
        rpm[i] = car.engine.rpm_min() + (car.engine.rpm_max() - car.engine.rpm_min()) * i / (N - 1);
        torque[i] = car.engine.torque(rpm[i]);
        power[i] = torque[i] * rpm[i] / 9549.0;
    }
    const auto& bx = car.engine.bsfc_rpm();
    std::vector<double> brpm(N), bsfc(N);
    for (int i = 0; i < N; ++i) {
        brpm[i] = bx.front() + (bx.back() - bx.front()) * i / (N - 1);
        bsfc[i] = car.engine.bsfc(brpm[i]);
    }
    const ImVec4 c0 = run_palette(0);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float half = avail.x * 0.5f - 4;

    auto rpm_marks = [&](bool horizontal) {
        const ImPlotInfLinesFlags fl = horizontal ? ImPlotInfLinesFlags_Horizontal : 0;
        std::vector<double> up(car.shift_rpm.begin(), car.shift_rpm.end());
        std::sort(up.begin(), up.end());
        up.erase(std::unique(up.begin(), up.end()), up.end());
        ImPlot::SetNextLineStyle(color_muted(), 1.0f);
        ImPlot::PlotInfLines("RPM de subida", up.data(), static_cast<int>(up.size()), fl);
        const double marks[] = {car.launch_rpm, car.downshift_rpm};
        ImPlot::SetNextLineStyle(ImVec4(0.6f, 0.6f, 0.6f, 0.6f), 1.0f);
        ImPlot::PlotInfLines("Largada / redução", marks, 2, fl);
    };

    const bool fit = needs_fit(4);
    if (ImPlot::BeginSubplots("##motor", 3, 1, ImVec2(half, avail.y), ImPlotSubplotFlags_LinkAllX)) {
        if (fit) ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("Torque")) {
            ImPlot::SetupAxes(nullptr, "Torque [N.m]", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
            ImPlot::SetNextLineStyle(c0, 2);
            ImPlot::PlotLine("Torque (pchip)", rpm.data(), torque.data(), N);
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4, c0);
            ImPlot::PlotScatter("Pontos medidos", car.engine.torque_rpm().data(), car.engine.torque_nm().data(),
                                static_cast<int>(car.engine.torque_rpm().size()));
            rpm_marks(false);
            ImPlot::EndPlot();
        }
        if (fit) ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("Potência")) {
            ImPlot::SetupAxes(nullptr, "Potência [kW]", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
            ImPlot::SetNextLineStyle(c0, 2);
            ImPlot::PlotLine("Potência", rpm.data(), power.data(), N);
            rpm_marks(false);
            ImPlot::EndPlot();
        }
        if (fit) ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("BSFC")) {
            ImPlot::SetupAxes("Rotação [rpm]", "BSFC [g/kWh]", 0, ImPlotAxisFlags_AutoFit);
            ImPlot::SetNextLineStyle(c0, 2);
            ImPlot::PlotLine("BSFC (linear)", brpm.data(), bsfc.data(), N);
            ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4, c0);
            ImPlot::PlotScatter("Pontos medidos##b", bx.data(), car.engine.bsfc_g_kwh().data(),
                                static_cast<int>(bx.size()));
            ImPlot::EndPlot();
        }
        ImPlot::EndSubplots();
    }
    ImGui::SameLine();
    // Rotação x velocidade em cada marcha (diagrama dente de serra)
    if (fit) ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("Rotação x velocidade por marcha", ImVec2(half, avail.y))) {
        ImPlot::SetupAxes("Velocidade [km/h]", "Rotação [rpm]", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
        for (int g = 1; g <= car.gear_count(); ++g) {
            double xs[2], ys[2];
            for (int k = 0; k < 2; ++k) {
                ys[k] = k == 0 ? car.engine.rpm_min() : car.shift_rpm[static_cast<std::size_t>(g - 1)];
                const double rpm1 = fsae::physics::engine_rpm(car, 1.0, g);  // rpm a 1 m/s
                xs[k] = ys[k] / rpm1 * 3.6;
            }
            ImPlot::SetNextLineStyle(run_palette(g - 1), 2.5f);
            ImPlot::PlotLine(("Marcha " + std::to_string(g)).c_str(), xs, ys, 2);
        }
        rpm_marks(true);
        ImPlot::EndPlot();
    }
}

// ---------------------------------------------------------------------------
void App::tab_corners() {
    if (no_runs(runs_)) return;
    const Run& r = runs_.back();
    ImGui::Text("Execução: %s", r.label.c_str());
    ImGui::SameLine();
    if (lap_combo(plot_lap_, max_laps())) ++view_gen_;

    // Velocidade real em cada segmento da volta escolhida (mínima dentro do segmento)
    const int lap = plot_lap_ == 0 ? r.result.laps : plot_lap_;
    const Series v = series(r, channel_index("velocity"), lap, true);
    std::vector<double> starts(r.segments.size() + 1, 0.0);
    for (std::size_t k = 0; k < r.segments.size(); ++k) starts[k + 1] = starts[k] + r.segments[k].length;
    std::vector<double> real_min(r.segments.size(), kNaN);
    for (std::size_t i = 0; i < v.x.size(); ++i) {
        const auto it = std::upper_bound(starts.begin(), starts.end(), v.x[i]);
        if (it == starts.begin() || it == starts.end()) continue;
        const std::size_t k = static_cast<std::size_t>(it - starts.begin()) - 1;
        real_min[k] = std::isnan(real_min[k]) ? v.y[i] : std::min(real_min[k], v.y[i]);
    }

    std::vector<std::size_t> corners;
    for (std::size_t k = 0; k < r.segments.size(); ++k) {
        if (r.segments[k].is_corner()) corners.push_back(k);
    }
    const int n = static_cast<int>(corners.size());
    if (n == 0) {
        ImGui::TextDisabled("Esta pista não tem curvas.");
        return;
    }
    std::vector<double> values(static_cast<std::size_t>(2 * n));
    std::vector<double> ticks(static_cast<std::size_t>(n));
    std::vector<std::string> labels(static_cast<std::size_t>(n));
    std::vector<const char*> label_ptrs(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        const std::size_t k = corners[static_cast<std::size_t>(i)];
        values[static_cast<std::size_t>(i)] = r.result.corner_speeds[k].v_max * 3.6;
        values[static_cast<std::size_t>(n + i)] = real_min[k];
        ticks[static_cast<std::size_t>(i)] = i;
        labels[static_cast<std::size_t>(i)] = std::to_string(k + 1);
    }
    for (int i = 0; i < n; ++i) label_ptrs[static_cast<std::size_t>(i)] = labels[static_cast<std::size_t>(i)].c_str();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (needs_fit(5)) ImPlot::SetNextAxesToFit();
    if (ImPlot::BeginPlot("Velocidade nas curvas", ImVec2(avail.x, avail.y * 0.55f))) {
        ImPlot::SetupAxes("Segmento", "Velocidade [km/h]", 0, ImPlotAxisFlags_AutoFit);
        if (n <= 60) ImPlot::SetupAxisTicks(ImAxis_X1, ticks.data(), n, label_ptrs.data());
        const char* items[] = {"Limite (aderência/tombamento)", "Real na volta"};
        ImPlot::SetupLegend(ImPlotLocation_North, ImPlotLegendFlags_Horizontal | ImPlotLegendFlags_Outside);
        std::vector<double> xa(static_cast<std::size_t>(n)), xb(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            xa[static_cast<std::size_t>(i)] = i - 0.19;
            xb[static_cast<std::size_t>(i)] = i + 0.19;
        }
        ImPlot::SetNextFillStyle(run_palette(0));
        ImPlot::PlotBars(items[0], xa.data(), values.data(), n, 0.36);
        ImPlot::SetNextFillStyle(run_palette(1));
        ImPlot::PlotBars(items[1], xb.data(), values.data() + n, n, 0.36);
        ImPlot::EndPlot();
    }
    if (ImGui::BeginTable("curvas", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                           ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        for (const char* h : {"Segmento", "Tipo", "Comprimento [m]", "Raio [m]", "Limite [km/h]", "Real [km/h]", "Limitada por"}) {
            ImGui::TableSetupColumn(h);
        }
        ImGui::TableHeadersRow();
        for (std::size_t k = 0; k < r.segments.size(); ++k) {
            const auto& s = r.segments[k];
            const auto& c = r.result.corner_speeds[k];
            ImGui::TableNextRow();
            const bool zero = s.is_corner() && !(c.v_max > 0);
            if (zero) ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(color_highlight_row()));
            ImGui::TableSetColumnIndex(0); ImGui::Text("%zu", k + 1);
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(s.type == fsae::track::SegmentType::Straight ? "reta" : s.type == fsae::track::SegmentType::Left ? "esquerda" : "direita");
            ImGui::TableSetColumnIndex(2); ImGui::Text("%.2f", s.length);
            ImGui::TableSetColumnIndex(3); if (s.is_corner()) ImGui::Text("%.2f", s.radius); else ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(4); if (s.is_corner()) ImGui::Text("%.2f", c.v_max * 3.6); else ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(5); if (!std::isnan(real_min[k])) ImGui::Text("%.2f", real_min[k]); else ImGui::TextDisabled("-");
            ImGui::TableSetColumnIndex(6);
            if (zero) ImGui::TextUnformatted("raio grande (v = 0)");
            else if (s.is_corner()) ImGui::TextUnformatted(c.limited_by_tipping() ? "tombamento" : "aderência");
        }
        ImGui::EndTable();
    }
}

// ---------------------------------------------------------------------------
void App::tab_compare() {
    if (no_runs(runs_)) return;
    if (compare_base_ >= static_cast<int>(runs_.size())) compare_base_ = 0;
    ImGui::SetNextItemWidth(260);
    if (ImGui::BeginCombo("Base", runs_[static_cast<std::size_t>(compare_base_)].label.c_str())) {
        for (int i = 0; i < static_cast<int>(runs_.size()); ++i) {
            if (ImGui::Selectable(runs_[static_cast<std::size_t>(i)].label.c_str(), i == compare_base_)) {
                compare_base_ = i;
                ++view_gen_;
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (lap_combo(plot_lap_, max_laps())) ++view_gen_;
    ImGui::SameLine();
    ImGui::TextDisabled("Delta positivo = mais lento que a base no mesmo ponto da pista");

    const Run& base = runs_[static_cast<std::size_t>(compare_base_)];
    const Series bt = series(base, 0, plot_lap_, true);
    const bool fit = needs_fit(6);
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (ImPlot::BeginSubplots("##comp", 2, 1, avail, ImPlotSubplotFlags_LinkAllX | ImPlotSubplotFlags_ShareItems)) {
        if (fit) ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("##v")) {
            ImPlot::SetupAxes(nullptr, "Velocidade [km/h]", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
            for (const auto& r : runs_) {
                if (!r.visible) continue;
                const Series s = series(r, channel_index("velocity"), plot_lap_, true);
                ImPlot::SetNextLineStyle(run_color(r), 2);
                ImPlot::PlotLine(item_id(r).c_str(), s.x.data(), s.y.data(), static_cast<int>(s.x.size()));
            }
            ImPlot::EndPlot();
        }
        if (fit) ImPlot::SetNextAxesToFit();
        if (ImPlot::BeginPlot("##dt")) {
            ImPlot::SetupAxes("Distância [m]", "Delta tempo [s]", 0, ImPlotAxisFlags_AutoFit);
            const double zero = 0;
            ImPlot::SetNextLineStyle(color_muted(), 1);
            ImPlot::PlotInfLines("##zero", &zero, 1, ImPlotInfLinesFlags_Horizontal);
            for (const auto& r : runs_) {
                if (!r.visible || &r == &base) continue;
                const Series st = series(r, 0, plot_lap_, true);
                if (st.x.empty() || bt.x.empty()) continue;
                const double dmax = std::min(st.x.back(), bt.x.back());
                std::vector<double> gx, gy;
                for (int i = 0; i <= 1000; ++i) {
                    const double d = dmax * i / 1000.0;
                    const double a = time_at(st, d), b = time_at(bt, d);
                    if (std::isnan(a) || std::isnan(b)) continue;
                    gx.push_back(d);
                    gy.push_back(a - b);
                }
                ImPlot::SetNextLineStyle(run_color(r), 2);
                ImPlot::PlotLine(item_id(r).c_str(), gx.data(), gy.data(), static_cast<int>(gx.size()));
            }
            ImPlot::EndPlot();
        }
        ImPlot::EndSubplots();
    }
}

// ---------------------------------------------------------------------------
void App::tab_runs() {
    if (no_runs(runs_)) return;
    ImGui::TextDisabled("Até 8 execuções ficam guardadas. Desmarque para esconder dos gráficos; "
                        "\"Usar carro\" copia o carro da execução para o editor.");
    const int max_l = max_laps();
    const int cols = 7 + max_l;
    if (ImGui::BeginTable("execuções", cols, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                                 ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollX)) {
        ImGui::TableSetupColumn("Mostrar");
        ImGui::TableSetupColumn("Nome", ImGuiTableColumnFlags_WidthFixed, 260);
        ImGui::TableSetupColumn("Pista");
        for (int k = 1; k <= max_l; ++k) ImGui::TableSetupColumn(("Volta " + std::to_string(k) + " [s]").c_str());
        ImGui::TableSetupColumn("Total [s]");
        ImGui::TableSetupColumn("V max [km/h]");
        ImGui::TableSetupColumn("Consumo [g]");
        ImGui::TableSetupColumn("Ações");
        ImGui::TableHeadersRow();
        std::size_t remove = runs_.size();
        for (std::size_t i = 0; i < runs_.size(); ++i) {
            Run& r = runs_[i];
            ImGui::PushID(r.id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            if (ImGui::Checkbox("##vis", &r.visible)) ++view_gen_;
            ImGui::SameLine();
            ImGui::ColorButton("##c", run_color(r), ImGuiColorEditFlags_NoTooltip, ImVec2(14, 14));
            ImGui::TableNextColumn();
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputText("##nome", &r.label);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(r.track_name.c_str());
            for (int k = 0; k < max_l; ++k) {
                ImGui::TableNextColumn();
                if (k < r.result.laps) ImGui::Text("%.3f", r.result.lap_times[static_cast<std::size_t>(k)]);
            }
            ImGui::TableNextColumn(); ImGui::Text("%.3f", r.result.total_time);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", r.result.max_velocity_kmh);
            ImGui::TableNextColumn(); ImGui::Text("%.2f", r.result.total_fuel_g);
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Usar carro")) {
                spec_ = r.spec;
                validate_car();
                status_ = "Carro da execução \"" + r.label + "\" copiado para o editor.";
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Salvar")) save_results(r);
            ImGui::SameLine();
            if (ImGui::SmallButton("Remover")) remove = i;
            ImGui::PopID();
        }
        ImGui::EndTable();
        if (remove < runs_.size()) {
            runs_.erase(runs_.begin() + static_cast<std::ptrdiff_t>(remove));
            ++view_gen_;
        }
    }
}

}  // namespace fsae::gui
