#include "fsae/physics/longitudinal.hpp"

#include <algorithm>
#include <cmath>

#include "fsae/physics/load_transfer.hpp"
#include "fsae/physics/powertrain.hpp"
#include "fsae/physics/resistances.hpp"

namespace fsae::physics {

Forces calculate_forces(const Car& car, double velocity, double slope, int& gear,
                        bool is_accelerating, control::ShiftState& shift, double dt) noexcept {
    constexpr double kPi = 3.141592653589793;
    Forces f;

    // 1. Forças resistivas
    f.downforce = downforce(car, velocity);
    f.drag = drag(car, velocity);
    f.rolling_resistance = tyre_resistance(car, velocity, slope, f.downforce);
    f.slope_force = slope_force(car, slope);

    // 2. Câmbio
    const int current_gear = gear;
    const int new_gear = control::manage_gear(car, velocity, current_gear, is_accelerating, shift, dt);
    gear = new_gear;

    // 3. Força do motor
    if (!shift.is_shifting) {
        double wheel_torque_nm = 0;
        const double launch_velocity = (2 * kPi / 60) * car.launch_rpm * car.tire_radius /
                                       (car.gear_ratios[current_gear - 1] * car.overall_reduction);
        if (velocity < launch_velocity) {
            const double engine_torque = launch_torque(car);
            const double gear_ratio = car.gear_ratios[new_gear - 1];
            wheel_torque_nm =
                engine_torque * gear_ratio * car.overall_reduction * car.powertrain_efficiency;
        } else {
            const double rpm = engine_rpm(car, velocity, new_gear);
            wheel_torque_nm = wheel_torque(car, rpm, new_gear);
        }
        f.engine_force = engine_force(car, wheel_torque_nm);
    }
    return f;
}

Step car_accelerate(const Car& car, double engine_force, double drag, double rolling_resistance,
                    double slope_force, double initial_velocity, double initial_position,
                    double dt, double slope) noexcept {
    double accel_guess = 0;
    double accel_new = 5;
    int iter_count = 0;
    constexpr int kMaxIters = 100;
    while (std::abs(accel_guess - accel_new) > 0.001 && iter_count < kMaxIters) {
        accel_guess = accel_new;
        const auto lt = load_transfer(car, accel_guess / car.g, 0, slope, initial_velocity);
        const double normal_rear = lt.normal_n[kRearRight] + lt.normal_n[kRearLeft];
        const double max_traction = max_grip(car, engine_force, normal_rear);
        const double actual_traction = std::min(engine_force, max_traction);
        const double net_force = actual_traction - drag - rolling_resistance - slope_force;
        accel_new = net_force / car.mass;
        ++iter_count;
    }

    Step s;
    s.acceleration = accel_new;
    s.velocity = initial_velocity + s.acceleration * dt;
    s.position = initial_position + initial_velocity * dt + (0.5 * s.acceleration * (dt * dt));
    return s;
}

BrakeStep car_brake(const Car& car, double brake_force, double drag, double rolling_resistance,
                    double slope_force, double initial_velocity, double initial_position,
                    double dt, double slope) noexcept {
    double accel_guess = 0;
    double accel_new = -5;
    int iter_count = 0;
    constexpr int kMaxIters = 300;
    double actual_brake_force = 0;
    while (std::abs(accel_guess - accel_new) > 0.001 && iter_count < kMaxIters) {
        accel_guess = accel_new;
        const auto lt = load_transfer(car, accel_guess / car.g, 0, slope, initial_velocity);
        const double total_normal = lt.normal_n[0] + lt.normal_n[1] + lt.normal_n[2] + lt.normal_n[3];
        const double max_braking_grip = car.mux_static * total_normal;
        actual_brake_force = std::min(brake_force, max_braking_grip);
        const double net_force = -(actual_brake_force + drag + rolling_resistance) - slope_force;
        accel_new = net_force / car.mass;
        ++iter_count;
    }

    BrakeStep s;
    s.actual_brake_force = actual_brake_force;
    s.acceleration = accel_new;
    s.velocity = initial_velocity + s.acceleration * dt;
    if (s.velocity < 0) s.velocity = 0;
    s.position = initial_position + initial_velocity * dt + (0.5 * s.acceleration * (dt * dt));
    return s;
}

}  // namespace fsae::physics
