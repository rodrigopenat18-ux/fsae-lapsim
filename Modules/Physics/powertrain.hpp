#pragma once

#include "Vehicle/car.hpp"

/// Motor, transmissão, freio e aderência longitudinal (Modules/Physics do MATLAB).
/// Marchas são numeradas a partir de 1, como no MATLAB.
namespace fsae::physics {

/// Rotação do motor [rpm] para a velocidade [m/s] na marcha `gear` (calculateEngineRPM).
[[nodiscard]] inline double engine_rpm(const Car& car, double velocity, int gear) noexcept {
    constexpr double kPi = 3.141592653589793;
    const double total_gear_ratio = car.gear_ratios[gear - 1] * car.overall_reduction;
    return (velocity / car.tire_radius) * total_gear_ratio * (60 / (2 * kPi));
}

/// Torque na roda [N·m] (calculateEngineTorque): torque do motor × relações × eficiência.
[[nodiscard]] inline double wheel_torque(const Car& car, double rpm, int gear) noexcept {
    const double engine_torque = car.engine.torque(rpm);
    return engine_torque * car.gear_ratios[gear - 1] * car.overall_reduction *
           car.powertrain_efficiency;
}

/// Força trativa [N] a partir do torque na roda (calculateEngineForce).
[[nodiscard]] inline double engine_force(const Car& car, double wheel_torque_nm) noexcept {
    return wheel_torque_nm / car.tire_radius;
}

/// Torque do motor [N·m] na rotação de largada ("two-step") (calculateLaunchTorque).
[[nodiscard]] inline double launch_torque(const Car& car) noexcept {
    return car.engine.torque(car.launch_rpm);
}

/// Força de frenagem [N] (calculateBrakeForce): 3000 N·m fixos no raio do pneu.
[[nodiscard]] inline double brake_force(const Car& car) noexcept { return 3000 / car.tire_radius; }

/// Aderência longitudinal máxima [N] nas rodas motrizes (calculateMaxGrip):
/// atrito estático, ou cinético se a força do motor passar do limite estático.
[[nodiscard]] inline double max_grip(const Car& car, double engine_force_n,
                                     double normal_driven_n) noexcept {
    const double max_static = car.mux_static * normal_driven_n;
    if (engine_force_n > max_static) return car.mux_kinetic * normal_driven_n;
    return max_static;
}

}  // namespace fsae::physics
