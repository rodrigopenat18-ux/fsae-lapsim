#include "Control/gearbox.hpp"

#include <algorithm>

#include "Physics/powertrain.hpp"

namespace fsae::control {

int find_initial_gear(const Car& car, double velocity) noexcept {
    if (velocity < 1.0) return 1;

    int best_gear = 1;
    double max_tractive_force = -1;
    const double rpm_min = car.engine.rpm_min();
    const double rpm_max = car.engine.rpm_max();
    for (int gear = 1; gear <= car.gear_count(); ++gear) {
        const double rpm = physics::engine_rpm(car, velocity, gear);
        if (rpm < rpm_min || rpm > rpm_max) continue;
        const double force = physics::engine_force(car, physics::wheel_torque(car, rpm, gear));
        if (force > max_tractive_force) {
            max_tractive_force = force;
            best_gear = gear;
        }
    }
    return best_gear;
}

int check_upshift(const Car& car, double velocity, int gear, bool is_shifting) noexcept {
    if (is_shifting || gear >= car.gear_count()) return gear;
    const double rpm = physics::engine_rpm(car, velocity, gear);
    return rpm > car.shift_rpm[gear - 1] ? gear + 1 : gear;
}

int check_downshift(const Car& car, double velocity, int gear, bool is_shifting) noexcept {
    if (gear <= 1 || is_shifting) return gear;
    const double rpm = physics::engine_rpm(car, velocity, gear);
    return rpm < car.downshift_rpm ? gear - 1 : gear;
}

int manage_gear(const Car& car, double velocity, int gear, bool is_accelerating, ShiftState& state,
                double dt) noexcept {
    if (state.is_shifting) {
        state.timer = state.timer - dt;
        if (state.timer <= 0) {
            state.is_shifting = false;
            state.timer = 0;
        }
        return gear;
    }

    const int candidate = is_accelerating ? check_upshift(car, velocity, gear, state.is_shifting)
                                          : check_downshift(car, velocity, gear, state.is_shifting);
    if (candidate != gear) {
        state.is_shifting = true;
        state.timer = car.shift_time;
    }
    return candidate;
}

double motor_rpm(const Car& car, double velocity, int gear) noexcept {
    return std::max(car.launch_rpm, physics::engine_rpm(car, velocity, gear));
}

}  // namespace fsae::control
