#pragma once

#include <cmath>

#include "fsae/vehicle/car.hpp"

/// Forças aerodinâmicas e resistivas (Modules/Physics do MATLAB).
/// As contas seguem a mesma ordem das expressões originais, para que o
/// arredondamento em ponto flutuante seja idêntico ao do MATLAB.
namespace fsae::physics {

/// Arrasto aerodinâmico [N] (calculateDrag).
[[nodiscard]] inline double drag(const Car& car, double velocity) noexcept {
    return 0.5 * car.rho * (velocity * velocity) * car.frontal_area * car.cd;
}

/// Downforce [N] (calculateDownforce).
[[nodiscard]] inline double downforce(const Car& car, double velocity) noexcept {
    return 0.5 * car.rho * (velocity * velocity) * car.frontal_area * car.cl;
}

/// Componente do peso ao longo da pista [N]; `slope` em radianos (calculateSlopeForce).
[[nodiscard]] inline double slope_force(const Car& car, double slope) noexcept {
    return (car.mass * car.g) * std::sin(slope);
}

/// Resistência ao rolamento [N] (calculateTyreResistance).
/// Coeficiente f = 0,01·(1 + v[km/h]/120), aplicado ao peso normal + downforce.
[[nodiscard]] inline double tyre_resistance(const Car& car, double velocity, double slope,
                                            double downforce_n) noexcept {
    const double ff = 0.01 * (1 + velocity * 3.6 / 120);
    return ff * (car.mass * car.g * std::cos(slope) + downforce_n);
}

}  // namespace fsae::physics
