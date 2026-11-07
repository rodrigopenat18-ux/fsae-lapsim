#pragma once

#include "Vehicle/car.hpp"

/// Modelo térmico de um disco de freio dianteiro (calculateBrakeTemp).
namespace fsae::physics {

struct BrakeTempResult {
    double new_temp = 0;  ///< temperatura após `dt` [°C]
    double p_in = 0;      ///< potência de frenagem recebida pelo disco [W]
    double p_conv = 0;    ///< perda por convecção [W]
    double p_rad = 0;     ///< perda por radiação [W]
    double p_cond = 0;    ///< perda por condução para o cubo [W]
    double h = 0;         ///< coeficiente de convecção [W/m²·K]
};

/// Avança a temperatura do disco por `dt` segundos.
///  - `brake_force_total` [N]: força de frenagem total do carro;
///  - `front_load_ratio`: fração que vai para o eixo dianteiro (MATLAB: padrão 0,60),
///    dividida igualmente entre os dois discos.
/// Convecção translacional + rotacional, radiação e condução, com calor específico
/// interpolado na temperatura atual.
[[nodiscard]] BrakeTempResult brake_temperature(const Car& car, double velocity,
                                                double brake_force_total, double current_temp,
                                                double dt, double front_load_ratio = 0.60);

}  // namespace fsae::physics
