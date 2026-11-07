#pragma once

#include "fsae/vehicle/car.hpp"

/// Lógica do câmbio (Modules/Control do MATLAB). Marchas numeradas a partir de 1.
namespace fsae::control {

/// Estado da troca de marcha (struct `shift_status` do MATLAB).
struct ShiftState {
    bool is_shifting = false;  ///< troca em andamento (isShifting)
    double timer = 0.0;        ///< tempo restante da troca [s] (timer)
};

/// Marcha com maior força trativa na velocidade dada, entre as que deixam o motor
/// dentro da faixa do mapa de torque; abaixo de 1 m/s, 1ª marcha (findInitialGear).
[[nodiscard]] int find_initial_gear(const Car& car, double velocity) noexcept;

/// Sobe uma marcha se o rpm passou do ponto de troca da marcha atual (checkUpshift).
[[nodiscard]] int check_upshift(const Car& car, double velocity, int gear,
                                bool is_shifting) noexcept;

/// Desce uma marcha se o rpm caiu abaixo de `downshift_rpm` (checkDownshift).
[[nodiscard]] int check_downshift(const Car& car, double velocity, int gear,
                                  bool is_shifting) noexcept;

/// Um passo de `dt` do câmbio (manageGear):
///  - durante uma troca, só desconta o tempo e mantém a marcha;
///  - senão, acelerando verifica subida e freando verifica redução; se trocar,
///    a nova marcha já vale e começa a contar `car.shift_time`.
/// Atualiza `state` e devolve a nova marcha.
[[nodiscard]] int manage_gear(const Car& car, double velocity, int gear, bool is_accelerating,
                              ShiftState& state, double dt) noexcept;

/// Rotação do motor [rpm] (updateEngineState): a maior entre a rotação de largada
/// ("two-step") e a imposta pelas rodas na marcha atual.
[[nodiscard]] double motor_rpm(const Car& car, double velocity, int gear) noexcept;

}  // namespace fsae::control
