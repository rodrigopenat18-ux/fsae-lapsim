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

/// Torque do motor [N·m] na rotação `rpm`. Com `car.model.limit_engine_map`, fora da faixa
/// medida: 0 acima da rotação máxima (limitador) e o valor do primeiro ponto abaixo da mínima.
[[nodiscard]] inline double engine_torque(const Car& car, double rpm) noexcept {
    if (car.model.limit_engine_map) {
        if (rpm > car.engine.rpm_max()) return 0.0;
        if (rpm < car.engine.rpm_min()) rpm = car.engine.rpm_min();
    }
    return car.engine.torque(rpm);
}

/// Consumo específico [g/kWh]. Com `car.model.limit_engine_map`, constante fora da faixa medida.
[[nodiscard]] inline double engine_bsfc(const Car& car, double rpm) noexcept {
    if (car.model.limit_engine_map) {
        const auto& x = car.engine.bsfc_rpm();
        rpm = rpm < x.front() ? x.front() : (rpm > x.back() ? x.back() : rpm);
    }
    return car.engine.bsfc(rpm);
}

/// Velocidade máxima que a transmissão permite [m/s]: última marcha na rotação máxima do motor.
[[nodiscard]] inline double max_vehicle_speed(const Car& car) noexcept {
    constexpr double kPi = 3.141592653589793;
    const double ratio = car.gear_ratios.back() * car.overall_reduction;
    return car.engine.rpm_max() / ratio * (2 * kPi / 60) * car.tire_radius;
}

/// Torque na roda [N·m] (calculateEngineTorque): torque do motor × relações × eficiência.
[[nodiscard]] inline double wheel_torque(const Car& car, double rpm, int gear) noexcept {
    const double tq = physics::engine_torque(car, rpm);
    return tq * car.gear_ratios[gear - 1] * car.overall_reduction *
           car.powertrain_efficiency;
}

/// Força trativa [N] a partir do torque na roda (calculateEngineForce).
[[nodiscard]] inline double engine_force(const Car& car, double wheel_torque_nm) noexcept {
    return wheel_torque_nm / car.tire_radius;
}

/// Torque do motor [N·m] na rotação de largada ("two-step") (calculateLaunchTorque).
[[nodiscard]] inline double launch_torque(const Car& car) noexcept {
    return engine_torque(car, car.launch_rpm);
}

/// Força de frenagem [N] (calculateBrakeForce): 3000 N·m fixos no raio do pneu.
[[nodiscard]] inline double brake_force(const Car& car) noexcept {
    return car.brake_max_torque / car.tire_radius;
}

/// Aderência longitudinal máxima [N] nas rodas motrizes (calculateMaxGrip):
/// atrito estático, ou cinético se a força do motor passar do limite estático.
[[nodiscard]] inline double max_grip(const Car& car, double engine_force_n,
                                     double normal_driven_n) noexcept {
    const double max_static = car.mux_static * normal_driven_n;
    if (engine_force_n > max_static) return car.mux_kinetic * normal_driven_n;
    return max_static;
}

}  // namespace fsae::physics
