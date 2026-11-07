#pragma once

#include <vector>

#include "Math/interp.hpp"

namespace fsae {

/// Mapas do motor: torque × rpm (pchip) e consumo específico BSFC × rpm (linear).
/// Equivale ao `dataEngine.m` (sem a curva de potência, que não era usada).
///
/// Atenção: fora da faixa medida os dois mapas são EXTRAPOLADOS, como no MATLAB
/// (ver docs/problemas_conhecidos.md, item 6).
class EngineMap {
public:
    EngineMap() = default;
    EngineMap(std::vector<double> rpm, std::vector<double> torque_nm,
              std::vector<double> bsfc_rpm, std::vector<double> bsfc_g_kwh);

    /// Torque do motor [N·m] (`car.TorqueInterpol(rpm)`).
    [[nodiscard]] double torque(double rpm) const noexcept { return torque_(rpm); }
    /// Consumo específico [g/kWh] (`car.bsfc_map(rpm)`).
    [[nodiscard]] double bsfc(double rpm) const noexcept { return bsfc_(rpm); }

    /// Menor e maior rpm da curva de torque (`car.rpm_min`, `car.rpm_max`).
    [[nodiscard]] double rpm_min() const noexcept { return rpm_min_; }
    [[nodiscard]] double rpm_max() const noexcept { return rpm_max_; }

    [[nodiscard]] const std::vector<double>& torque_rpm() const noexcept { return torque_.x(); }
    [[nodiscard]] const std::vector<double>& torque_nm() const noexcept { return torque_.y(); }
    [[nodiscard]] const std::vector<double>& bsfc_rpm() const noexcept { return bsfc_.x(); }
    [[nodiscard]] const std::vector<double>& bsfc_g_kwh() const noexcept { return bsfc_.y(); }

private:
    math::PchipInterpolant torque_;
    math::LinearInterpolant bsfc_;
    double rpm_min_ = 0.0;
    double rpm_max_ = 0.0;
};

}  // namespace fsae
