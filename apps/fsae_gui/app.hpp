#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

#include <imgui.h>

#include "fsae/sim/lap.hpp"
#include "fsae/track/geometry.hpp"
#include "fsae/track/track.hpp"
#include "fsae/vehicle/car.hpp"

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

/// Série de uma volta (ou de todas) pronta para plotar.
struct Series {
    std::vector<double> x, y;
};

class App {
public:
    App();

    /// Desenha um quadro da interface inteira.
    void frame();

    /// Modo captura (desenvolvimento): seleciona uma aba e roda uma simulação.
    void select_tab(const std::string& name) { forced_tab_ = name; }
    void run_now() { run_simulation(false); }
    /// Demonstração (capturas de tela): segunda execução com Cl·A +20%.
    void demo_second_run() {
        spec_.cl_a_m2 *= 1.2;
        spec_.name += " (ClA +20%)";
        run_simulation(false);
    }
    void load_track_by_name(const std::string& name);

    [[nodiscard]] ImVec4 run_color(const Run& r) const;

private:
    // --- Painel esquerdo ---
    void panel_setup();
    bool car_editor();      ///< devolve true se algo mudou
    bool track_selector();  ///< idem
    void panel_results();

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
    void load_car(const std::filesystem::path& p);
    void load_track(const std::filesystem::path& p);
    void save_results(const Run& r);
    void refresh_lists();
    void validate_car();

    /// Série de um canal de uma execução, para a volta `lap` (0 = todas) e eixo X
    /// tempo (`x_is_distance == false`) ou distância, com tempo/distância da volta
    /// começando em zero.
    [[nodiscard]] Series series(const Run& r, int channel, int lap, bool x_is_distance) const;
    [[nodiscard]] Series series_xy(const Run& r, int cx, int cy, int lap) const;
    [[nodiscard]] Run* current_run();
    [[nodiscard]] int max_laps() const;

    // --- Estado ---
    fsae::CarSpec spec_;
    std::string car_file_;   ///< vazio = carro padrão / não salvo
    std::string car_error_;
    bool car_valid_ = true;

    fsae::track::Track track_;
    std::string track_file_;
    std::string track_error_;
    int event_ = 0;  ///< índice em kEvents
    int laps_ = 2;
    bool laps_auto_ = true;
    int start_segment_ = 1;

    std::vector<std::filesystem::path> track_files_;
    std::vector<std::filesystem::path> car_files_;

    std::vector<Run> runs_;
    int next_run_id_ = 1;
    bool auto_run_ = false;
    std::string run_error_;
    std::string status_;

    // Opções dos gráficos
    int plot_lap_ = -1;             ///< -1 = última volta, 0 = todas, k = volta k
    bool x_distance_ = false;
    std::array<bool, 32> tel_selected_{};
    int map_channel_ = 0;
    int map_run_ = -1;              ///< índice em runs_, -1 = última
    int xy_x_ = 0, xy_y_ = 0;
    bool xy_lines_ = false;
    int compare_base_ = 0;
    std::string forced_tab_;

    /// Sobe sempre que os dados exibidos mudam (nova execução, outra volta...); cada aba
    /// reajusta os eixos uma vez por geração e depois deixa o usuário dar zoom.
    int view_gen_ = 0;
    std::array<int, 16> fitted_gen_ = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
    bool needs_fit(int tab);
};

}  // namespace fsae::gui
