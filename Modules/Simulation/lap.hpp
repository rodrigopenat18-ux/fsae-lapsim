#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>

#include "Tracks/corner_speed.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"

/// Simulação de uma ou mais voltas (runSimulationLoop + pós-processamento do runSimulation).
namespace fsae::sim {

/// Provas. Define o número padrão de voltas (MATLAB: 2 em autocross/pista customizada,
/// 1 em skidpad/aceleração; endurance: 25 voltas seguidas, largada parada).
enum class Event { Autocross, Skidpad, Acceleration, Endurance, Custom };

[[nodiscard]] int default_laps(Event event) noexcept;
[[nodiscard]] const char* to_string(Event event) noexcept;
/// "autocross", "skidpad", "acceleration", "endurance" ou "custom" (lança std::invalid_argument).
[[nodiscard]] Event parse_event(const std::string& name);

/// Modelo de simulação.
enum class Solver {
    /// Modelo original (idêntico ao MATLAB): curvas a velocidade constante, retas no tempo.
    Original,
    /// Modelo melhorado: quase-estático em distância, com elipse de atrito
    /// (Simulation/quasi_static.hpp).
    QuasiStatic,
};

[[nodiscard]] const char* to_string(Solver solver) noexcept;
/// "original" ou "melhorado"/"improved"/"quasi-static" (lança std::invalid_argument).
[[nodiscard]] Solver parse_solver(const std::string& name);

struct SimulationOptions {
    int laps = 1;                     ///< voltas seguidas (a 1ª parte parada)
    double initial_velocity = 0;      ///< [m/s] (MATLAB: fixo em 0)
    double initial_brake_temp = 25;   ///< [°C] (MATLAB: fixo em 25)
    std::ostream* log = nullptr;      ///< se não nulo, imprime o relatório trecho a trecho
    /// Modelo usado. A biblioteca usa o original por padrão; a interface e a linha de
    /// comando usam o melhorado.
    Solver solver = Solver::Original;
    /// Correções pontuais do modelo original (padrão: nenhuma). Ver Vehicle/model_options.hpp.
    /// O modelo melhorado ignora este campo e usa sempre todas as correções.
    ModelOptions model;
    /// Passo de distância do modelo melhorado [m].
    double step_m = 0.25;
    /// Combustível no tanque na largada [kg]. No modelo melhorado a massa cai a cada volta
    /// com o combustível gasto (tanque suposto no CG); no original entra como massa
    /// constante. 0 = massa do carro como está.
    double fuel_load_kg = 0;
};

/// Telemetria da simulação inteira, um elemento por amostra.
/// Mesmos campos e ordem do `SimOut.History` do MATLAB.
struct Telemetry {
    std::vector<double> time;                ///< [s]
    std::vector<double> distance;            ///< [m] acumulada desde a largada
    std::vector<double> velocity;            ///< [m/s]
    std::vector<int> gear;                   ///< marcha (a partir de 1)
    std::vector<double> acceleration;        ///< longitudinal [m/s²] (0 nas curvas)
    std::vector<double> rpm;                 ///< [rpm]
    std::vector<double> lateral_accel;       ///< [m/s²] (negativo em curva à esquerda)
    std::vector<double> drag;                ///< [N]
    std::vector<double> downforce;           ///< [N]
    std::vector<double> rolling_resistance;  ///< [N]
    std::vector<double> slope_force;         ///< [N]
    std::vector<double> engine_force;        ///< [N]
    std::vector<double> brake_force;         ///< [N]
    std::vector<double> brake_temp;          ///< [°C]
    std::vector<double> h_coef;              ///< [W/m²·K]
    std::vector<double> fuel_consumed_g;     ///< acumulado [g]
    std::vector<double> p_conv;              ///< [W]
    std::vector<double> p_rad;               ///< [W]
    std::vector<double> p_cond;              ///< [W]
    std::vector<double> load_front_dist;     ///< [% no eixo dianteiro]

    [[nodiscard]] std::size_t size() const noexcept { return time.size(); }
    void reserve(std::size_t n);
};

struct SimulationResult {
    Telemetry history;
    int laps = 1;
    double track_length = 0;  ///< comprimento de UMA volta [m]

    // --- SimOut.Results ---
    double total_time = 0;      ///< soma dos tempos dos trechos [s] (TotalTime)
    double max_velocity_kmh = 0;  ///< (MaxVelocity)
    double max_lat_g = 0;       ///< (MaxLatG)
    double max_brake_temp = 0;  ///< [°C] (MaxBrakeTemp)
    double total_fuel_g = 0;    ///< (TotalFuel_g)

    /// Tempo de cada volta [s] (Lap1_Time, Lap2_Time…). As voltas são separadas pela
    /// primeira amostra com distância ≥ k × comprimento da volta (ver docs/problemas_conhecidos.md,
    /// item 3). NaN se a volta não fechou.
    std::vector<double> lap_times;
    std::vector<double> lap_fuel_g;            ///< consumo de cada volta [g]
    std::vector<std::size_t> lap_start_index;  ///< primeira amostra de cada volta
    std::vector<double> lap_mass_kg;           ///< massa do carro usada em cada volta [kg]
    /// Volta mais rápida (FastestLap): a menor entre a 2ª e a última volta (no MATLAB,
    /// com 2 voltas, é a 2ª); com 1 volta, o tempo total.
    double fastest_lap = 0;

    /// Velocidade máxima de cada segmento de UMA volta (initializeTrack).
    std::vector<track::CornerSpeed> corner_speeds;
};

/// Simula `options.laps` voltas seguidas da pista `segments` (uma volta).
[[nodiscard]] SimulationResult simulate(const Car& car, const std::vector<track::Segment>& segments,
                                        const SimulationOptions& options = {});

}  // namespace fsae::sim
