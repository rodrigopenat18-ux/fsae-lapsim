#pragma once

#include <array>

#include "carro/car.hpp"

/// Cargas normais nos pneus (calculateStaticLoads / calculateDynamicTransfer /
/// calculateLoadTransfer).
///
/// Ordem dos pneus, igual ao MATLAB (sentido horário a partir do dianteiro esquerdo):
///   [0] DE  dianteiro esquerdo   [1] DD  dianteiro direito
///   [2] TD  traseiro direito     [3] TE  traseiro esquerdo
///
/// Acelerações em g: `ax_g > 0` acelerando, `ax_g < 0` freando; `ay_g` lateral
/// (ver docs/problemas_conhecidos.md sobre a convenção de sinal lateral).
namespace fsae::physics {

using WheelLoads = std::array<double, 4>;

enum Wheel : int { kFrontLeft = 0, kFrontRight = 1, kRearRight = 2, kRearLeft = 3 };

/// Carga estática por pneu [N] com o carro parado na rampa `slope` [rad].
[[nodiscard]] WheelLoads static_loads(const Car& car, double slope) noexcept;

/// Variação de carga por pneu [N] devida às acelerações longitudinal e lateral.
[[nodiscard]] WheelLoads dynamic_transfer(const Car& car, double ax_g, double ay_g) noexcept;

struct LoadTransferResult {
    WheelLoads normal_n{};      ///< carga normal total por pneu [N] (Normal_N)
    WheelLoads distribution{};  ///< parcela de cada pneu na soma [%] (DistrMassa_perc)
};

/// Estática + transferência dinâmica + downforce dividido pelo centro de pressão.
[[nodiscard]] LoadTransferResult load_transfer(const Car& car, double ax_g, double ay_g,
                                               double slope, double velocity) noexcept;

/// Parcelas que não dependem da aceleração (carga estática e aerodinâmica), para
/// reaproveitar quando a mesma rampa e velocidade são avaliadas com várias acelerações
/// (iteração de ponto fixo do carAccelerate/carBrake). O resultado é idêntico ao de
/// `load_transfer`.
struct LoadBase {
    WheelLoads static_n{};
    WheelLoads aero_n{};
};
[[nodiscard]] LoadBase load_base(const Car& car, double slope, double velocity) noexcept;
/// Carga normal por pneu = estática + dinâmica + aero (mesma ordem de soma do MATLAB).
[[nodiscard]] WheelLoads normal_loads(const Car& car, const LoadBase& base, double ax_g,
                                      double ay_g) noexcept;

}  // namespace fsae::physics
