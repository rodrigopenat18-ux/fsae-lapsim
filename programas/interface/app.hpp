#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "simulacao/lap.hpp"
#include "pista/geometry.hpp"
#include "pista/track.hpp"
#include "carro/car.hpp"
#include "import_dialog.hpp"
#include "theme.hpp"

namespace fsae::gui {

/// Um canal da telemetria, já convertido para a unidade de exibição.
struct ChannelInfo {
    const char* key;    ///< nome da coluna no telemetry.csv
    const char* label;  ///< nome na interface
    const char* unit;
    bool stairs;        ///< desenhar em degraus (marcha)
};

/// Lista fixa de canais (mesma ordem de `Run::channels`).
const std::vector<ChannelInfo>& channels();
int channel_index(const char* key);

/// Uma simulação executada, guardada para comparar com as outras.
struct Run {
    int id = 0;
    std::string label;
    std::string track_name;
    std::string car_name;
    std::string event;
    fsae::CarSpec spec;
    std::vector<fsae::track::Segment> segments;  ///< já com o segmento de largada
    fsae::sim::SimulationResult result;
    fsae::track::TrackGeometry geometry;
    std::vector<std::vector<double>> channels;   ///< canais convertidos (km/h, g...)
    std::vector<int> lap;                        ///< volta de cada amostra (1..n)
    std::vector<std::size_t> zero_speed_corners; ///< segmentos com v_max = 0
    bool visible = true;
    int color_slot = 0;
    double sim_ms = 0;
};

/// Mapas de cores disponíveis para o mapa da pista (definidos em plots.cpp).
int colormap_count();
const char* colormap_name(int i);

/// Série de uma volta (ou de todas) pronta para plotar.
struct Series {
    std::vector<double> x, y;
};

class App {
public:
    App();
    ~App();

    /// Desenha um quadro da interface inteira.
    void frame();

    // --- Modo captura (desenvolvimento/documentação) ---
    void select_tab(const std::string& name) { forced_tab_ = name; }
    void select_left_tab(const std::string& name) { forced_left_tab_ = name; }
    void open_editor(const std::string& tab) { show_editor_ = true; forced_editor_tab_ = tab; }
    void run_now() { run_simulation(false); }
    void load_track_by_name(const std::string& name);
    void set_theme(Theme t);
    void set_persist(bool p) { persist_ = p; }
    void set_map_colormap(const std::string& name, bool invert) {
        for (int i = 0; i < colormap_count(); ++i) {
            if (name == colormap_name(i)) map_cmap_ = i;
        }
        map_invert_ = invert;
    }
    /// Segunda execução com Cl·A +20%.
    void demo_import(const std::filesystem::path& p) { import_.open_file(ImportTarget::Torque, p); }
    void demo_second_run() {
        spec_.cl_a_m2 *= 1.2;
        spec_.name += " (ClA +20%)";
        run_simulation(false);
    }

    [[nodiscard]] ImVec4 run_color(const Run& r) const { return run_palette(r.color_slot); }

private:
    // --- Estrutura ---
    void menu_bar();
    void panel_simulation();
    void panel_cars();
    void panel_results();
    bool track_selector();

    // --- Editor do carro (car_editor.cpp) ---
    void car_editor_window();
    bool editor_general();
    bool editor_aero();
    bool editor_chassis();
    bool editor_transmission();
    bool editor_engine();
    bool editor_brakes();
    void car_summary();

    // --- Abas de gráficos (plots.cpp) ---
    void tab_telemetry();
    void tab_track_map();
    void tab_gg();
    void tab_xy();
    void tab_engine();
    void tab_corners();
    void tab_compare();
    void tab_runs();

    // --- Ações ---
    void run_simulation(bool replace_last);
    void on_car_changed();
    enum class CarAction { None, New, Default, Load, Duplicate };
    void request_car_action(CarAction a, const std::filesystem::path& p = {});
    void do_car_action(CarAction a, const std::filesystem::path& p);
    void confirm_discard_modal();
    bool save_car(bool save_as);
    [[nodiscard]] bool car_modified() const;
    void mark_car_saved();
    void open_car_dialog();
    void open_track_dialog();
    void load_track(const std::filesystem::path& p);
    void save_results(const Run& r);
    void refresh_lists();
    void validate_car();
    void import_from_file(ImportTarget t);
    void import_from_clipboard(ImportTarget t);
    void load_settings();
    void save_settings() const;

    [[nodiscard]] Series series(const Run& r, int channel, int lap, bool x_is_distance) const;
    [[nodiscard]] Series series_xy(const Run& r, int cx, int cy, int lap) const;
    [[nodiscard]] Run* current_run();
    [[nodiscard]] int max_laps() const;
    bool needs_fit(int tab);

    // --- Carro ---
    fsae::CarSpec spec_;
    std::string car_file_;    ///< vazio = ainda não salvo em arquivo
    std::string saved_json_;  ///< estado salvo/carregado, para saber se há alterações
    std::string car_error_;
    bool car_valid_ = true;
    bool show_editor_ = false;
    std::string forced_editor_tab_;
    CarAction pending_action_ = CarAction::None;
    std::filesystem::path pending_path_;
    bool open_discard_modal_ = false;
    ImportDialog import_;

    // --- Pista ---
    fsae::track::Track track_;
    std::string track_file_;
    std::string track_error_;
    int event_ = 0;
    int laps_ = 2;
    bool laps_auto_ = true;
    int start_segment_ = 1;
    std::vector<std::filesystem::path> track_files_;
    std::vector<std::filesystem::path> car_files_;
    std::vector<std::string> car_names_;  ///< nome do carro dentro de cada arquivo

    // --- Execuções ---
    std::vector<Run> runs_;
    int next_run_id_ = 1;
    bool auto_run_ = false;
    std::string run_error_;
    std::string status_;

    // --- Opções de exibição ---
    Theme theme_ = Theme::Light;
    bool persist_ = true;
    int plot_lap_ = -1;  ///< -1 = última volta, 0 = todas, k = volta k
    bool x_distance_ = false;
    std::array<bool, 32> tel_selected_{};
    int map_channel_ = 0;
    int map_run_ = -1;
    int map_cmap_ = 0;   ///< índice em kColormaps (plots.cpp)
    bool map_invert_ = false;
    float map_width_ = 8.0f;
    int xy_x_ = 0, xy_y_ = 0;
    bool xy_lines_ = false;
    int compare_base_ = 0;
    std::string forced_tab_;
    std::string forced_left_tab_;

    int view_gen_ = 0;
    std::array<int, 16> fitted_gen_ = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
};

}  // namespace fsae::gui
