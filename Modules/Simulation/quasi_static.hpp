#pragma once

#include <vector>

#include "Physics/load_transfer.hpp"
#include "Simulation/lap.hpp"
#include "Tracks/corner_speed.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"

/// Modelo melhorado: simulação quase-estática em distância.
///
/// A pista é dividida em passos de distância (`SimulationOptions::step_m`, com um ponto em
/// cada fronteira de segmento). Em cada segmento calcula-se a velocidade-limite de curva;
/// uma passada de trás para frente monta o envelope de frenagem e uma passada para frente
/// acelera com o motor, o câmbio e o tempo de troca, sem passar do envelope. Aceleração,
/// frenagem e curva dividem a aderência pela elipse de atrito, eixo por eixo:
///
///  - a força lateral se divide entre os eixos pelo equilíbrio de momentos em regime
///    (Fy_d = m·ay·(1 − w), Fy_t = m·ay·w, w = fração do peso atrás);
///  - cada pneu tem μ = μ₀·(1 − k·(Fz/Fz_estática − 1)) (k = sensibilidade à carga);
///  - a força longitudinal disponível num eixo é Fx_max·sqrt(1 − (Fy/Fy_max)²);
///  - tração só no eixo traseiro, com controle de tração ideal (sem queda para μ cinético);
///  - freio nos dois eixos: distribuição ideal (cada eixo no seu limite, seguindo a
///    transferência de carga longitudinal) ou balanço fixo, limitado pelo torque máximo.
///
/// A inclinação dos segmentos é tratada em radianos e o mapa do motor é limitado à faixa
/// medida (o modelo melhorado usa sempre `ModelOptions::corrected()`).
namespace fsae::sim {

/// Simula com o modelo melhorado (chamado por `simulate` quando
/// `options.solver == Solver::QuasiStatic`).
[[nodiscard]] SimulationResult simulate_quasi_static(const Car& car,
                                                     const std::vector<track::Segment>& segments,
                                                     const SimulationOptions& options);

/// Igual, com um carro por volta (`lap_cars.size()` voltas). Usado no endurance, em que a
/// massa cai a cada volta com o combustível gasto.
[[nodiscard]] SimulationResult simulate_quasi_static(const std::vector<Car>& lap_cars,
                                                     const std::vector<track::Segment>& segments,
                                                     const SimulationOptions& options);

/// Peças do modelo melhorado, expostas para os testes.
namespace qs {

/// Cargas e capacidade dos pneus de cada eixo num estado.
struct AxleState {
    double cap_x_front = 0, cap_x_rear = 0;  ///< aderência longitudinal máxima [N]
    double cap_y_front = 0, cap_y_rear = 0;  ///< aderência lateral máxima [N]
    double fy_front = 0, fy_rear = 0;        ///< força lateral pedida a cada eixo [N]
    physics::WheelLoads normal{};            ///< carga normal de cada pneu [N]
};

/// Estado com aceleração longitudinal `ax` [m/s², + acelerando] e lateral `ay` [m/s², ≥ 0]
/// na rampa `slope` [rad].
[[nodiscard]] AxleState axle_state(const Car& car, double v, double ax, double ay, double slope);

/// Fração da aderência longitudinal que sobra com a lateral `fy`: sqrt(1 − (fy/cap_y)²),
/// 0 se a lateral já esgota o pneu.
[[nodiscard]] double ellipse_factor(double fy, double cap_y) noexcept;

/// Arrasto + rolamento + componente do peso na rampa [N] (positivo freia o carro).
[[nodiscard]] double resistance(const Car& car, double v, double slope) noexcept;

/// Velocidade-limite de uma curva de raio `radius` [m] e inclinação `slope` [rad]:
/// aderência lateral dos dois eixos, com a força para vencer as resistências dentro da
/// elipse (`v_grip`), e nenhuma roda descarregada (`v_tip`). Infinito se a curva não limita
/// até a velocidade máxima do carro.
[[nodiscard]] track::CornerSpeed corner_limit(const Car& car, double radius, double slope);

/// Força do motor nas rodas [N] na marcha `gear`; abaixo da rotação de largada a embreagem
/// patina nela ("two-step").
[[nodiscard]] double drive_force(const Car& car, double v, int gear) noexcept;

/// Marcha com maior força do motor entre as que não passam do rpm de troca.
[[nodiscard]] int best_gear(const Car& car, double v) noexcept;

/// Aceleração máxima [m/s²] com a força de motor `engine_force` [N], na curvatura
/// `curvature` [1/m]: a tração é limitada pela aderência traseira que sobra da curva.
[[nodiscard]] double max_acceleration(const Car& car, double v, double curvature, double slope,
                                      double engine_force);

struct Braking {
    double decel = 0;           ///< desaceleração máxima [m/s², positiva]
    double brake_force = 0;     ///< força de freio total nessa condição [N]
    double front_fraction = 0;  ///< parcela da força de freio no eixo dianteiro
};

/// Frenagem máxima na curvatura `curvature` [1/m], com a distribuição de freio do carro.
[[nodiscard]] Braking max_braking(const Car& car, double v, double curvature, double slope);

/// Maior uso da elipse de atrito entre os eixos, (Fx/Fx_max)² + (Fy/Fy_max)², com
/// aceleração `ax`, lateral `ay`, força do motor `engine_force` (eixo traseiro) e força de
/// freio `brake_force` (dividida como no modelo). 1 = pneu no limite.
[[nodiscard]] double friction_usage(const Car& car, double v, double ax, double ay, double slope,
                                    double engine_force, double brake_force);

}  // namespace qs

}  // namespace fsae::sim
