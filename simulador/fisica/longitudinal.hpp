#pragma once

#include "cambio/gearbox.hpp"
#include "carro/car.hpp"

/// Dinâmica longitudinal de um passo de tempo (calculateForces / carAccelerate / carBrake).
namespace fsae::physics {

/// Forças atuando no carro num instante (saídas de calculateForces).
struct Forces {
    double engine_force = 0;        ///< força trativa disponível do motor [N] (0 durante a troca)
    double drag = 0;                ///< arrasto [N]
    double rolling_resistance = 0;  ///< resistência ao rolamento [N]
    double slope_force = 0;         ///< componente do peso na rampa [N]
    double downforce = 0;           ///< downforce [N]
};

/// Forças resistivas + câmbio + força do motor (calculateForces).
///
/// Chama `control::manage_gear`, que atualiza `gear` e `shift`. Durante a troca a força
/// do motor é zero. Abaixo da velocidade em que o motor chega a `launch_rpm` na marcha
/// de ENTRADA, usa o torque de largada ("two-step") com a relação da marcha nova;
/// acima, o torque do mapa na rotação da marcha nova.
[[nodiscard]] Forces calculate_forces(const Car& car, double velocity, double slope, int& gear,
                                      bool is_accelerating, control::ShiftState& shift,
                                      double dt) noexcept;

/// Resultado de um passo de integração.
struct Step {
    double position = 0;      ///< posição ao fim do passo [m]
    double velocity = 0;      ///< velocidade ao fim do passo [m/s]
    double acceleration = 0;  ///< aceleração usada no passo [m/s²]
};

/// Passo acelerando (carAccelerate). A aceleração é obtida por iteração de ponto fixo
/// (até 100 iterações, critério 0,001 m/s²), pois a tração disponível depende da carga
/// no eixo traseiro, que depende da própria aceleração.
[[nodiscard]] Step car_accelerate(const Car& car, double engine_force, double drag,
                                  double rolling_resistance, double slope_force,
                                  double initial_velocity, double initial_position, double dt,
                                  double slope) noexcept;

struct BrakeStep : Step {
    double actual_brake_force = 0;  ///< força de freio efetiva, limitada pela aderência [N]
};

/// Passo freando (carBrake). Iteração de ponto fixo (até 300 iterações) com a aderência
/// das quatro rodas. A velocidade final não fica negativa.
[[nodiscard]] BrakeStep car_brake(const Car& car, double brake_force, double drag,
                                  double rolling_resistance, double slope_force,
                                  double initial_velocity, double initial_position, double dt,
                                  double slope) noexcept;

}  // namespace fsae::physics
