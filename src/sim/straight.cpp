#include "fsae/sim/straight.hpp"

#include <cmath>
#include <utility>

#include "fsae/control/gearbox.hpp"
#include "fsae/physics/brake_thermal.hpp"
#include "fsae/physics/load_transfer.hpp"
#include "fsae/physics/longitudinal.hpp"
#include "fsae/physics/powertrain.hpp"

namespace fsae::sim {

void StraightHistory::reserve(std::size_t n) {
    for (auto* v : {&time, &position, &velocity, &acceleration, &rpm, &drag, &downforce,
                    &rolling_resistance, &slope_force, &engine_force, &brake_force, &brake_temp,
                    &h_coef, &fuel_consumed_g, &p_conv, &p_rad, &p_cond, &load_front_dist}) {
        v->reserve(n);
    }
    gear.reserve(n);
}

namespace {

/// Uma linha da telemetria.
struct Sample {
    double time, position, velocity, acceleration;
    int gear;
    double rpm, drag, downforce, rolling_resistance, slope_force, engine_force, brake_force,
        brake_temp, h_coef, fuel_consumed_g, p_conv, p_rad, p_cond, load_front_dist;
};

void push(StraightHistory& h, const Sample& s) {
    h.time.push_back(s.time);
    h.position.push_back(s.position);
    h.velocity.push_back(s.velocity);
    h.acceleration.push_back(s.acceleration);
    h.gear.push_back(s.gear);
    h.rpm.push_back(s.rpm);
    h.drag.push_back(s.drag);
    h.downforce.push_back(s.downforce);
    h.rolling_resistance.push_back(s.rolling_resistance);
    h.slope_force.push_back(s.slope_force);
    h.engine_force.push_back(s.engine_force);
    h.brake_force.push_back(s.brake_force);
    h.brake_temp.push_back(s.brake_temp);
    h.h_coef.push_back(s.h_coef);
    h.fuel_consumed_g.push_back(s.fuel_consumed_g);
    h.p_conv.push_back(s.p_conv);
    h.p_rad.push_back(s.p_rad);
    h.p_cond.push_back(s.p_cond);
    h.load_front_dist.push_back(s.load_front_dist);
}

/// Estado inicial da reta (primeira linha do histórico).
StraightHistory start_history(double velocity, int gear, double motor_rpm, double brake_temp,
                              double static_front_ratio) {
    StraightHistory h;
    h.reserve(512);
    push(h, {0, 0, velocity, 0, gear, motor_rpm, 0, 0, 0, 0, 0, 0, brake_temp, 0, 0, 0, 0, 0,
             static_front_ratio * 100});
    return h;
}

/// Combustível gasto em `dt` com o motor em `motor_rpm` a plena carga [g] (BSFC).
double fuel_step(const Car& car, double motor_rpm, double dt) {
    const double torque_motor = car.engine.torque(motor_rpm);
    const double power_kw = (torque_motor * motor_rpm) / 9549;
    const double bsfc = car.engine.bsfc(motor_rpm);
    return (bsfc * power_kw * dt) / 3600;
}

/// Fração da carga normal no eixo dianteiro para a aceleração `acc`, ou `fallback`
/// se a carga total não for positiva.
double front_ratio(const Car& car, double acc, double slope, double velocity, double fallback) {
    const auto lt = physics::load_transfer(car, acc / car.g, 0, slope, velocity);
    const auto& n = lt.normal_n;
    const double total = n[0] + n[1] + n[2] + n[3];
    return total > 0 ? (n[0] + n[1]) / total : fallback;
}

/// MODO 1: aceleração máxima até o fim da reta.
StraightResult full_throttle(const Car& car, double length, double v0, double slope,
                             double temp0) {
    constexpr double dt = kStraightDt;
    const double static_front_ratio = (1 - car.weight_dist_rear_percent / 100);

    double pos = 0, vel = v0, time = 0, fuel = 0, temp = temp0;
    int gear = control::find_initial_gear(car, vel);
    double motor_rpm = control::motor_rpm(car, vel, gear);
    StraightHistory h = start_history(vel, gear, motor_rpm, temp, static_front_ratio);
    control::ShiftState shift;

    while (pos < length) {
        const auto f = physics::calculate_forces(car, vel, slope, gear, true, shift, dt);
        const auto step = physics::car_accelerate(car, f.engine_force, f.drag,
                                                  f.rolling_resistance, f.slope_force, vel, pos,
                                                  dt, slope);
        fuel = fuel + fuel_step(car, motor_rpm, dt);
        const double ratio = front_ratio(car, step.acceleration, slope, vel, static_front_ratio);
        const auto bt = physics::brake_temperature(car, vel, 0, temp, dt, ratio);
        temp = bt.new_temp;

        pos = step.position;
        vel = step.velocity;
        time = time + dt;
        motor_rpm = control::motor_rpm(car, vel, gear);

        push(h, {time, pos, vel, step.acceleration, gear, motor_rpm, f.drag, f.downforce,
                 f.rolling_resistance, f.slope_force, f.engine_force, 0, temp, bt.h, fuel,
                 bt.p_conv, bt.p_rad, bt.p_cond, ratio * 100});
    }
    return {time, std::move(h)};
}

/// MODO 2: bissecção do ponto de frenagem para chegar ao fim com velocidade ≤ alvo.
StraightResult with_target(const Car& car, double length, double v0, double target, double slope,
                           double temp0) {
    constexpr double dt = kStraightDt;
    constexpr double kPrecision = 0.05;
    constexpr double kVelTolerance = 0.1;
    constexpr int kMaxIterations = 100;
    const double static_front_ratio = (1 - car.weight_dist_rear_percent / 100);

    double low_bp = 0.0;
    double high_bp = length;
    bool has_safe = false;  // MATLAB: total_time_safe == -1
    double total_time_safe = 0;
    StraightHistory history_safe;

    double time = 0;
    StraightHistory h;

    for (int iter = 1; iter <= kMaxIterations; ++iter) {
        const double braking_point = (low_bp + high_bp) / 2.0;
        double pos = 0, vel = v0, fuel = 0, temp = temp0;
        time = 0;
        int gear = control::find_initial_gear(car, vel);
        double motor_rpm = control::motor_rpm(car, vel, gear);
        h = start_history(vel, gear, motor_rpm, temp, static_front_ratio);
        control::ShiftState shift;
        bool running = true;
        bool stopped_early = false;

        while (running) {
            const bool accelerating = (pos < braking_point);
            const auto f = physics::calculate_forces(car, vel, slope, gear, accelerating, shift, dt);

            double curr_eng = 0, curr_brk = 0, fuel_inc = 0;
            double ratio = static_front_ratio;
            physics::Step step;
            if (accelerating) {
                curr_eng = f.engine_force;
                step = physics::car_accelerate(car, f.engine_force, f.drag, f.rolling_resistance,
                                               f.slope_force, vel, pos, dt, slope);
                fuel_inc = fuel_step(car, motor_rpm, dt);
                ratio = front_ratio(car, step.acceleration, slope, vel, static_front_ratio);
            } else {
                const auto bs = physics::car_brake(car, physics::brake_force(car), f.drag,
                                                   f.rolling_resistance, f.slope_force, vel, pos,
                                                   dt, slope);
                step = bs;
                curr_brk = bs.actual_brake_force;
                ratio = front_ratio(car, step.acceleration, slope, vel, 0.5);
            }

            fuel = fuel + fuel_inc;
            const auto bt = physics::brake_temperature(car, vel, curr_brk, temp, dt, ratio);
            temp = bt.new_temp;

            pos = step.position;
            vel = step.velocity;
            time = time + dt;
            motor_rpm = control::motor_rpm(car, vel, gear);

            push(h, {time, pos, vel, step.acceleration, gear, motor_rpm, f.drag, f.downforce,
                     f.rolling_resistance, f.slope_force, curr_eng, curr_brk, temp, bt.h, fuel,
                     bt.p_conv, bt.p_rad, bt.p_cond, ratio * 100});

            if (pos >= length) {
                running = false;
            } else if (vel <= 0.1 && !accelerating) {
                running = false;
                stopped_early = true;
            }
        }

        const double final_vel = vel;
        const double diff_vel = final_vel - target;

        if (stopped_early) {
            low_bp = braking_point;
            if (!has_safe) {
                has_safe = true;
                total_time_safe = time;
                history_safe = h;
            }
        } else if (final_vel <= target) {
            low_bp = braking_point;
            has_safe = true;
            total_time_safe = time;
            history_safe = h;
            if (std::abs(diff_vel) < kVelTolerance) break;
        } else {
            high_bp = braking_point;
        }
        if ((high_bp - low_bp) < kPrecision) break;
    }

    if (!has_safe) return {time, std::move(h)};
    return {total_time_safe, std::move(history_safe)};
}

}  // namespace

StraightResult simulate_straight(const Car& car, double length, double initial_velocity,
                                 double target_velocity, double slope, double initial_brake_temp) {
    if (std::isinf(target_velocity)) {
        return full_throttle(car, length, initial_velocity, slope, initial_brake_temp);
    }
    return with_target(car, length, initial_velocity, target_velocity, slope, initial_brake_temp);
}

}  // namespace fsae::sim
