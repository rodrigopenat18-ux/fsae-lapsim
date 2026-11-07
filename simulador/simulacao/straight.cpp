#include "simulacao/straight.hpp"

#include <cassert>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "cambio/gearbox.hpp"
#include "fisica/brake_thermal.hpp"
#include "fisica/load_transfer.hpp"
#include "fisica/longitudinal.hpp"
#include "fisica/powertrain.hpp"

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

/// Limite de segurança: 10.000 s simulados numa reta (o MATLAB ficaria preso para sempre,
/// por exemplo numa subida que o carro não vence).
constexpr std::size_t kMaxSteps = 1000000;

[[noreturn]] void throw_stuck() {
    throw std::runtime_error(
        "reta: o carro nao consegue chegar ao fim (mais de 10000 s simulados)");
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

    std::size_t steps = 0;
    while (pos < length) {
        if (++steps > kMaxSteps) throw_stuck();
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

/// Resultado de uma tentativa de ponto de frenagem.
struct Attempt {
    double time = 0;
    double final_vel = 0;
    bool stopped_early = false;
};

/// Simula a reta com frenagem a partir de `braking_point`.
///
/// Com `h == nullptr` só integra o movimento (posição, velocidade, câmbio): é o que a
/// bissecção usa para decidir. Temperatura do disco, combustível e distribuição de carga
/// não influenciam o movimento, então só são calculados quando há histórico para gravar.
/// As duas formas dão exatamente o mesmo tempo e a mesma velocidade final.
Attempt run_braking(const Car& car, double length, double v0, double slope, double temp0,
                    double braking_point, double static_front_ratio, StraightHistory* h) {
    constexpr double dt = kStraightDt;
    double pos = 0, vel = v0, fuel = 0, temp = temp0, time = 0;
    int gear = control::find_initial_gear(car, vel);
    double motor_rpm = control::motor_rpm(car, vel, gear);
    if (h) *h = start_history(vel, gear, motor_rpm, temp, static_front_ratio);
    control::ShiftState shift;
    std::size_t steps = 0;
    const double brake_force = physics::brake_force(car);

    while (true) {
        if (++steps > kMaxSteps) throw_stuck();
        const bool accelerating = (pos < braking_point);
        const auto f = physics::calculate_forces(car, vel, slope, gear, accelerating, shift, dt);

        double curr_eng = 0, curr_brk = 0, fuel_inc = 0;
        double ratio = static_front_ratio;
        physics::Step step;
        if (accelerating) {
            curr_eng = f.engine_force;
            step = physics::car_accelerate(car, f.engine_force, f.drag, f.rolling_resistance,
                                           f.slope_force, vel, pos, dt, slope);
            if (h) {
                fuel_inc = fuel_step(car, motor_rpm, dt);
                ratio = front_ratio(car, step.acceleration, slope, vel, static_front_ratio);
            }
        } else {
            const auto bs = physics::car_brake(car, brake_force, f.drag, f.rolling_resistance,
                                               f.slope_force, vel, pos, dt, slope);
            step = bs;
            curr_brk = bs.actual_brake_force;
            if (h) ratio = front_ratio(car, step.acceleration, slope, vel, 0.5);
        }

        physics::BrakeTempResult bt;
        if (h) {
            fuel = fuel + fuel_inc;
            bt = physics::brake_temperature(car, vel, curr_brk, temp, dt, ratio);
            temp = bt.new_temp;
        }

        pos = step.position;
        vel = step.velocity;
        time = time + dt;

        if (h) {
            motor_rpm = control::motor_rpm(car, vel, gear);
            push(*h, {time, pos, vel, step.acceleration, gear, motor_rpm, f.drag, f.downforce,
                      f.rolling_resistance, f.slope_force, curr_eng, curr_brk, temp, bt.h, fuel,
                      bt.p_conv, bt.p_rad, bt.p_cond, ratio * 100});
        }

        if (pos >= length) return {time, vel, false};
        if (vel <= 0.1 && !accelerating) return {time, vel, true};
    }
}

/// Estado dinâmico completo entre passos (o que determina o passo seguinte).
struct DynState {
    double pos = 0, vel = 0, time = 0;
    int gear = 1;
    control::ShiftState shift;
};

/// Trecho em aceleração máxima, compartilhado por todas as tentativas da bissecção.
///
/// Toda tentativa acelera a partir do mesmo estado inicial até a primeira amostra com
/// posição ≥ ponto de frenagem; até ali as contas são idênticas, então esse trecho é
/// integrado uma vez só e cada tentativa começa a frear do estado guardado. O resultado
/// é bit a bit o mesmo de simular cada tentativa desde o início.
class FullThrottlePrefix {
public:
    FullThrottlePrefix(const Car& car, double length, double v0, double slope)
        : car_(car), length_(length), slope_(slope) {
        DynState s;
        s.vel = v0;
        s.gear = control::find_initial_gear(car, v0);
        states_.reserve(1024);
        states_.push_back(s);
    }

    /// Índice da primeira amostra com pos ≥ `bp` (onde a tentativa começa a frear), ou da
    /// primeira que alcança o fim da reta, integrando mais passos se preciso.
    /// Busca linear: não supõe que a posição seja crescente.
    std::size_t first_at_or_after(double bp) {
        for (std::size_t k = 0;; ++k) {
            if (k == states_.size()) step();
            const double p = states_[k].pos;
            if (p >= bp || (k > 0 && p >= length_)) return k;
        }
    }

    [[nodiscard]] const DynState& state(std::size_t k) const { return states_[k]; }

private:
    void step() {
        constexpr double dt = kStraightDt;
        DynState s = states_.back();
        if (states_.size() > kMaxSteps) throw_stuck();
        const auto f = physics::calculate_forces(car_, s.vel, slope_, s.gear, true, s.shift, dt);
        const auto st = physics::car_accelerate(car_, f.engine_force, f.drag, f.rolling_resistance,
                                                f.slope_force, s.vel, s.pos, dt, slope_);
        s.pos = st.position;
        s.vel = st.velocity;
        s.time = s.time + dt;
        states_.push_back(s);
    }

    const Car& car_;
    double length_, slope_;
    std::vector<DynState> states_;
};

/// Continua uma tentativa a partir de `s` (já em frenagem) até o fim da reta ou a parada.
Attempt finish_braking(const Car& car, double length, double slope, double braking_point,
                       DynState s) {
    constexpr double dt = kStraightDt;
    const double brake_force = physics::brake_force(car);
    for (std::size_t n = 0;; ++n) {
        if (n > kMaxSteps) throw_stuck();
        const bool accelerating = (s.pos < braking_point);
        const auto f = physics::calculate_forces(car, s.vel, slope, s.gear, accelerating, s.shift, dt);
        physics::Step st;
        if (accelerating) {
            st = physics::car_accelerate(car, f.engine_force, f.drag, f.rolling_resistance,
                                         f.slope_force, s.vel, s.pos, dt, slope);
        } else {
            st = physics::car_brake(car, brake_force, f.drag, f.rolling_resistance, f.slope_force,
                                    s.vel, s.pos, dt, slope);
        }
        s.pos = st.position;
        s.vel = st.velocity;
        s.time = s.time + dt;
        if (s.pos >= length) return {s.time, s.vel, false};
        if (s.vel <= 0.1 && !accelerating) return {s.time, s.vel, true};
    }
}

/// MODO 2: bissecção do ponto de frenagem para chegar ao fim com velocidade ≤ alvo.
StraightResult with_target(const Car& car, double length, double v0, double target, double slope,
                           double temp0) {
    constexpr double kPrecision = 0.05;
    constexpr double kVelTolerance = 0.1;
    constexpr int kMaxIterations = 100;
    const double static_front_ratio = (1 - car.weight_dist_rear_percent / 100);

    double low_bp = 0.0;
    double high_bp = length;
    bool has_safe = false;  // MATLAB: total_time_safe == -1
    double total_time_safe = 0;
    double safe_bp = 0;     // ponto de frenagem do resultado "seguro"
    double last_time = 0, last_bp = 0;
    FullThrottlePrefix prefix(car, length, v0, slope);

    for (int iter = 1; iter <= kMaxIterations; ++iter) {
        const double braking_point = (low_bp + high_bp) / 2.0;
        // Trecho acelerando (compartilhado) + frenagem a partir do ponto de frenagem.
        const std::size_t k = prefix.first_at_or_after(braking_point);
        const DynState& s = prefix.state(k);
        const Attempt a = (k > 0 && s.pos >= length)
                              ? Attempt{s.time, s.vel, false}
                              : finish_braking(car, length, slope, braking_point, s);
        last_time = a.time;
        last_bp = braking_point;
        const double diff_vel = a.final_vel - target;

        if (a.stopped_early) {
            low_bp = braking_point;
            if (!has_safe) {
                has_safe = true;
                total_time_safe = a.time;
                safe_bp = braking_point;
            }
        } else if (a.final_vel <= target) {
            low_bp = braking_point;
            has_safe = true;
            total_time_safe = a.time;
            safe_bp = braking_point;
            if (std::abs(diff_vel) < kVelTolerance) break;
        } else {
            high_bp = braking_point;
        }
        if ((high_bp - low_bp) < kPrecision) break;
    }

    // Repete a tentativa escolhida gravando o histórico completo (mesmo resultado).
    StraightResult r;
    const double bp = has_safe ? safe_bp : last_bp;
    const Attempt a = run_braking(car, length, v0, slope, temp0, bp, static_front_ratio, &r.history);
    r.total_time = has_safe ? total_time_safe : last_time;
    assert(a.time == r.total_time);  // a repetição é determinística
    (void)a;
    return r;
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
