#pragma once

#include <cstddef>
#include <limits>
#include <vector>

#include "carro/car.hpp"

/// Simulação de uma reta no tempo (calculate_Straight do MATLAB).
namespace fsae::sim {

/// Passo de tempo fixo da integração da reta [s].
inline constexpr double kStraightDt = 0.01;

/// Telemetria da reta, um elemento por passo (o primeiro é o estado inicial).
/// Mesmos campos e ordem do `history` do MATLAB.
struct StraightHistory {
    std::vector<double> time;                ///< [s] desde o início da reta
    std::vector<double> position;            ///< [m] desde o início da reta
    std::vector<double> velocity;            ///< [m/s]
    std::vector<double> acceleration;        ///< [m/s²]
    std::vector<int> gear;                   ///< marcha (a partir de 1)
    std::vector<double> rpm;                 ///< rotação do motor [rpm]
    std::vector<double> drag;                ///< [N]
    std::vector<double> downforce;           ///< [N]
    std::vector<double> rolling_resistance;  ///< [N]
    std::vector<double> slope_force;         ///< [N]
    std::vector<double> engine_force;        ///< força do motor aplicada [N] (0 freando)
    std::vector<double> brake_force;         ///< força de freio efetiva [N]
    std::vector<double> brake_temp;          ///< temperatura do disco [°C]
    std::vector<double> h_coef;              ///< coef. de convecção do disco [W/m²·K]
    std::vector<double> fuel_consumed_g;     ///< combustível acumulado [g]
    std::vector<double> p_conv;              ///< perda por convecção do disco [W]
    std::vector<double> p_rad;               ///< perda por radiação do disco [W]
    std::vector<double> p_cond;              ///< perda por condução do disco [W]
    std::vector<double> load_front_dist;     ///< carga no eixo dianteiro [% do total]

    [[nodiscard]] std::size_t size() const noexcept { return time.size(); }
    void reserve(std::size_t n);
};

struct StraightResult {
    double total_time = 0;  ///< tempo para percorrer a reta [s]
    StraightHistory history;
};

/// Simula uma reta de `length` metros partindo de `initial_velocity`.
///
///  - `target_velocity` infinito: acelera o tempo todo até passar do fim da reta;
///  - finito: procura por bissecção o ponto de frenagem para chegar ao fim com
///    velocidade ≤ alvo (precisão 0,05 m, tolerância 0,1 m/s, até 100 iterações).
///
/// A integração para no primeiro passo em que a posição ≥ `length`, então a reta
/// pode ser "ultrapassada" em até um passo (ver docs/problemas_conhecidos.md).
/// `slope` em radianos; `initial_brake_temp` em °C.
[[nodiscard]] StraightResult simulate_straight(const Car& car, double length,
                                               double initial_velocity, double target_velocity,
                                               double slope, double initial_brake_temp = 25);

/// Atalho para `target_velocity` infinito (aceleração máxima).
inline constexpr double kNoTarget = std::numeric_limits<double>::infinity();

}  // namespace fsae::sim
