#include "Simulation/quasi_static.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <ostream>
#include <stdexcept>

#include "Control/gearbox.hpp"
#include "Physics/brake_thermal.hpp"
#include "Physics/powertrain.hpp"
#include "Physics/resistances.hpp"

namespace fsae::sim {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kLiftLoad = 0.01;  ///< carga [N] abaixo da qual a roda conta como levantada

/// Raiz de g(x) = 0 por iteração de ponto fixo x ← f(x); se não convergir (ganho ≥ 1),
/// bisseção em [lo, hi] com g(lo) > 0 > g(hi), g(x) = f(x) − x.
template <typename F>
double solve_fixed_point(F f, double x0, double lo, double hi) {
    double x = x0;
    for (int it = 0; it < 60; ++it) {
        const double nx = f(x);
        if (std::abs(nx - x) < 1e-10) return nx;
        x = nx;
    }
    for (int it = 0; it < 200 && hi - lo > 1e-10; ++it) {
        const double mid = 0.5 * (lo + hi);
        if (f(mid) - mid > 0) lo = mid; else hi = mid;
    }
    return 0.5 * (lo + hi);
}

template <typename... Args>
void logf(std::ostream* log, const char* fmt, Args... args) {
    if (log == nullptr) return;
    char buf[512];
    std::snprintf(buf, sizeof buf, fmt, args...);
    *log << buf;
}

}  // namespace

namespace qs {

AxleState axle_state(const Car& car, double v, double ax, double ay, double slope) {
    AxleState s;
    const auto base = physics::load_base(car, slope, v);
    s.normal = physics::normal_loads(car, base, ax / car.g, ay / car.g);

    const double w = car.weight_dist_rear_percent / 100;
    const double fz0_front = car.mass * car.g * (1 - w) / 2;  // carga nominal do pneu
    const double fz0_rear = car.mass * car.g * w / 2;
    const double k = car.tyre_load_sensitivity;
    for (int i = 0; i < 4; ++i) {
        const double fz = std::max(0.0, s.normal[static_cast<std::size_t>(i)]);
        const bool front = i == physics::kFrontLeft || i == physics::kFrontRight;
        const double fz0 = front ? fz0_front : fz0_rear;
        const double factor = std::max(0.0, 1 - k * (fz / fz0 - 1));
        const double cx = car.mux_static * factor * fz;
        const double cy = car.muy * factor * fz;
        (front ? s.cap_x_front : s.cap_x_rear) += cx;
        (front ? s.cap_y_front : s.cap_y_rear) += cy;
    }
    s.fy_front = car.mass * ay * (1 - w);
    s.fy_rear = car.mass * ay * w;
    return s;
}

double ellipse_factor(double fy, double cap_y) noexcept {
    if (fy <= 0) return 1;
    if (cap_y <= 0) return 0;
    const double r = fy / cap_y;
    return r >= 1 ? 0 : std::sqrt(1 - r * r);
}

double resistance(const Car& car, double v, double slope) noexcept {
    const double down = physics::downforce(car, v);
    return physics::drag(car, v) + physics::tyre_resistance(car, v, slope, down) +
           physics::slope_force(car, slope);
}

namespace {

bool grip_ok(const Car& car, double v, double radius, double slope) {
    const double ay = v * v / radius;
    const AxleState s = axle_state(car, v, 0, ay, slope);
    if (s.fy_front > s.cap_y_front || s.fy_rear > s.cap_y_rear) return false;
    const double ef = ellipse_factor(s.fy_front, s.cap_y_front);
    const double er = ellipse_factor(s.fy_rear, s.cap_y_rear);
    const double needed = resistance(car, v, slope);  // força para manter a velocidade
    if (needed >= 0) return needed <= s.cap_x_rear * er;
    return -needed <= s.cap_x_front * ef + s.cap_x_rear * er;  // descida: segura no freio
}

bool tip_ok(const Car& car, double v, double radius, double slope) {
    const double ay = v * v / radius;
    const AxleState s = axle_state(car, v, 0, ay, slope);
    for (double n : s.normal) {
        if (n <= kLiftLoad) return false;
    }
    return true;
}

/// Primeira velocidade (varrendo de 0 para cima) em que `ok` falha; infinito se não falhar
/// até `v_cap`.
template <typename Ok>
double first_failure(Ok ok, double v_cap) {
    constexpr double kScan = 0.5;
    double good = 0;
    for (double v = kScan;; v += kScan) {
        const double vv = std::min(v, v_cap);
        if (!ok(vv)) {
            double lo = good, hi = vv;
            while (hi - lo > 1e-9) {
                const double mid = 0.5 * (lo + hi);
                if (ok(mid)) lo = mid; else hi = mid;
            }
            return lo;
        }
        good = vv;
        if (vv >= v_cap) return kInf;
    }
}

}  // namespace

track::CornerSpeed corner_limit(const Car& car, double radius, double slope) {
    track::CornerSpeed c;
    if (radius == 0) {
        c.v_max = c.v_grip = c.v_tip = kInf;
        return c;
    }
    const double r = std::abs(radius);
    const double v_cap = physics::max_vehicle_speed(car);
    c.v_grip = first_failure([&](double v) { return grip_ok(car, v, r, slope); }, v_cap);
    c.v_tip = first_failure([&](double v) { return tip_ok(car, v, r, slope); }, v_cap);
    c.v_max = std::min(c.v_grip, c.v_tip);
    return c;
}

double drive_force(const Car& car, double v, int gear) noexcept {
    const double rpm = control::motor_rpm(car, v, gear);  // max(two-step, rpm das rodas)
    return physics::engine_force(car, physics::wheel_torque(car, rpm, gear));
}

int best_gear(const Car& car, double v) noexcept {
    int best = 1;
    double best_force = -1;
    for (int g = 1; g <= car.gear_count(); ++g) {
        const double rpm = physics::engine_rpm(car, v, g);
        const double limit = g < car.gear_count() ? car.shift_rpm[static_cast<std::size_t>(g - 1)]
                                                  : car.engine.rpm_max();
        if (rpm > limit) continue;
        const double f = drive_force(car, v, g);
        if (f > best_force) {
            best_force = f;
            best = g;
        }
    }
    if (best_force < 0) best = car.gear_count();  // acima de todos os pontos de troca
    return best;
}

namespace {

/// O eixo dianteiro segura a força lateral com a aceleração longitudinal `ax`?
bool front_lateral_ok(const Car& car, double v, double ax, double ay, double slope) {
    const AxleState s = axle_state(car, v, ax, ay, slope);
    return s.fy_front <= s.cap_y_front;
}

/// O eixo traseiro segura a força lateral com a aceleração longitudinal `ax`?
bool rear_lateral_ok(const Car& car, double v, double ax, double ay, double slope) {
    const AxleState s = axle_state(car, v, ax, ay, slope);
    return s.fy_rear <= s.cap_y_rear;
}

/// Maior x em [lo, hi] com ok(x) verdadeiro, supondo ok decrescente em x (lo se nenhum).
template <typename Ok>
double largest_ok(Ok ok, double lo, double hi) {
    if (ok(hi)) return hi;
    if (!ok(lo)) return lo;
    while (hi - lo > 1e-9) {
        const double mid = 0.5 * (lo + hi);
        if (ok(mid)) lo = mid; else hi = mid;
    }
    return lo;
}

}  // namespace

double max_acceleration(const Car& car, double v, double curvature, double slope,
                        double engine_force) {
    const double ay = v * v * std::abs(curvature);
    const double res = resistance(car, v, slope);
    auto f = [&](double a) {
        const AxleState s = axle_state(car, v, a, ay, slope);
        const double avail = s.cap_x_rear * ellipse_factor(s.fy_rear, s.cap_y_rear);
        return (std::min(engine_force, avail) - res) / car.mass;
    };
    const double lo = -res / car.mass - 1;
    const double hi = (std::max(engine_force, 0.0) - res) / car.mass + 1;
    const double a = solve_fixed_point(f, 0.0, lo, hi);
    if (ay <= 0 || a <= 0) return a;
    // Acelerando, a carga vai para trás e a dianteira perde aderência lateral (sai de
    // frente): a aceleração fica limitada ao ponto em que a dianteira ainda segura a curva.
    // (A traseira já é limitada pela elipse acima.)
    return largest_ok([&](double x) { return front_lateral_ok(car, v, x, ay, slope); }, 0.0, a);
}

namespace {

/// Força de freio total possível e parcela dianteira num estado.
void brake_capacity(const Car& car, const AxleState& s, double& force, double& front_fraction) {
    const double af = s.cap_x_front * ellipse_factor(s.fy_front, s.cap_y_front);
    const double ar = s.cap_x_rear * ellipse_factor(s.fy_rear, s.cap_y_rear);
    if (car.brake_bias_front) {
        const double b = *car.brake_bias_front;
        force = std::min(af / b, ar / (1 - b));  // trava primeiro o eixo que chega antes ao limite
        front_fraction = b;
    } else {
        force = af + ar;  // distribuição ideal: os dois eixos no limite
        front_fraction = force > 0 ? af / force : 1 - car.weight_dist_rear_percent / 100;
    }
    force = std::min(force, physics::brake_force(car));  // torque máximo do sistema
}

}  // namespace

Braking max_braking(const Car& car, double v, double curvature, double slope) {
    const double ay = v * v * std::abs(curvature);
    const double res = resistance(car, v, slope);
    auto f = [&](double d) {
        const AxleState s = axle_state(car, v, -d, ay, slope);
        double force = 0, ff = 0;
        brake_capacity(car, s, force, ff);
        return (force + res) / car.mass;
    };
    const double hi = (physics::brake_force(car) + std::max(res, 0.0)) / car.mass + 1;
    Braking b;
    b.decel = solve_fixed_point(f, car.mux_static * car.g, std::min(0.0, res / car.mass) - 1, hi);
    if (ay > 0 && b.decel > 0) {
        // Freando, a carga vai para a frente e a traseira perde aderência lateral (sai de
        // traseira): a desaceleração fica limitada ao ponto em que a traseira ainda segura.
        // No limite da curva isso pode ficar abaixo da desaceleração só com as resistências;
        // aí o piloto mantém um pouco de acelerador (a força de freio sai zero).
        b.decel = largest_ok([&](double d) { return rear_lateral_ok(car, v, -d, ay, slope); },
                             0.0, b.decel);
    }
    const AxleState s = axle_state(car, v, -b.decel, ay, slope);
    brake_capacity(car, s, b.brake_force, b.front_fraction);
    b.brake_force = std::min(b.brake_force, std::max(0.0, car.mass * b.decel - res));
    return b;
}

double friction_usage(const Car& car, double v, double ax, double ay, double slope,
                      double engine_force, double brake_force) {
    const AxleState s = axle_state(car, v, ax, std::abs(ay), slope);
    double fx_front = 0, fx_rear = std::max(0.0, engine_force);
    if (brake_force > 0) {
        double cap = 0, ff = 0;
        brake_capacity(car, s, cap, ff);
        fx_front += brake_force * ff;
        fx_rear += brake_force * (1 - ff);
    }
    auto usage = [](double fx, double cx, double fy, double cy) {
        const double ux = fx <= 0 ? 0 : (cx <= 0 ? kInf : fx / cx);
        const double uy = fy <= 0 ? 0 : (cy <= 0 ? kInf : fy / cy);
        return ux * ux + uy * uy;
    };
    return std::max(usage(fx_front, s.cap_x_front, s.fy_front, s.cap_y_front),
                    usage(fx_rear, s.cap_x_rear, s.fy_rear, s.cap_y_rear));
}

}  // namespace qs

namespace {

/// Um passo de distância da malha.
struct Step {
    double ds = 0;         ///< comprimento [m]
    double curvature = 0;  ///< 1/R [1/m], 0 em reta
    double sign = 1;       ///< sinal da aceleração lateral na telemetria (−1 à esquerda)
    double slope = 0;      ///< [rad]
    std::size_t lap = 0;
};

}  // namespace

SimulationResult simulate_quasi_static(const Car& car, const std::vector<track::Segment>& segments,
                                       const SimulationOptions& options) {
    if (options.laps < 1) throw std::invalid_argument("numero de voltas deve ser >= 1");
    const auto laps = static_cast<std::size_t>(options.laps);
    std::vector<Car> cars(laps, car);
    if (!(options.fuel_load_kg > 0)) return simulate_quasi_static(cars, segments, options);

    // Massa caindo com o combustível: 1ª passada com o tanque cheio em todas as voltas;
    // a 2ª usa, em cada volta, o combustível restante no meio dela pelo consumo da 1ª.
    // (O consumo por volta muda ~0,1% com a massa, então uma correção basta.)
    for (auto& c : cars) c.mass = car.mass + options.fuel_load_kg;
    SimulationOptions quiet = options;
    quiet.log = nullptr;
    const SimulationResult first = simulate_quasi_static(cars, segments, quiet);
    double used = 0;
    for (std::size_t k = 0; k < laps; ++k) {
        const double lap_kg = first.lap_fuel_g[k] / 1000;
        const double remaining_mid = std::max(0.0, options.fuel_load_kg - used - 0.5 * lap_kg);
        cars[k].mass = car.mass + remaining_mid;
        used += lap_kg;
    }
    SimulationResult r = simulate_quasi_static(cars, segments, options);
    if (r.total_fuel_g / 1000 > options.fuel_load_kg) {
        logf(options.log, "[AVISO] o combustivel acaba antes do fim: %.3f kg gastos de %.3f kg\n",
             r.total_fuel_g / 1000, options.fuel_load_kg);
    }
    return r;
}

SimulationResult simulate_quasi_static(const std::vector<Car>& lap_cars_in,
                                       const std::vector<track::Segment>& segments,
                                       const SimulationOptions& options) {
    if (segments.empty()) throw std::invalid_argument("pista sem segmentos");
    if (lap_cars_in.empty()) throw std::invalid_argument("numero de voltas deve ser >= 1");
    if (!(options.step_m > 0)) throw std::invalid_argument("passo de distancia deve ser > 0");
    std::ostream* log = options.log;

    std::vector<Car> cars = lap_cars_in;
    for (auto& c : cars) c.model = ModelOptions::corrected();
    const std::size_t laps = cars.size();

    SimulationResult r;
    r.laps = static_cast<int>(laps);
    double lap_length = 0;
    for (const auto& s : segments) lap_length += s.length;
    r.track_length = lap_length;

    // --- Limites de curva (por volta: a massa pode mudar) ---
    std::vector<std::vector<double>> seg_limit(laps, std::vector<double>(segments.size(), kInf));
    for (std::size_t lap = 0; lap < laps; ++lap) {
        if (lap > 0 && cars[lap].mass == cars[lap - 1].mass) {  // (os carros so diferem na massa)
            seg_limit[lap] = seg_limit[lap - 1];
            continue;
        }
        for (std::size_t k = 0; k < segments.size(); ++k) {
            const auto c = qs::corner_limit(cars[lap], segments[k].radius, segments[k].slope);
            seg_limit[lap][k] = c.v_max;
            if (lap == 0) r.corner_speeds.push_back(c);
        }
    }

    // --- Malha em distância: um ponto em cada fronteira de segmento ---
    std::vector<Step> steps;
    std::vector<double> node_limit{kInf};
    std::vector<double> node_dist{0.0};
    std::vector<std::size_t> lap_node(laps + 1, 0);
    for (std::size_t lap = 0; lap < laps; ++lap) {
        lap_node[lap] = node_dist.size() - 1;
        double seg_start = 0;
        for (std::size_t k = 0; k < segments.size(); ++k) {
            const auto& sg = segments[k];
            const double seg_end = seg_start + sg.length;
            if (sg.length > 0) {
                const auto n = static_cast<std::size_t>(
                    std::max(1.0, std::ceil(sg.length / options.step_m - 1e-9)));
                Step st;
                st.ds = sg.length / static_cast<double>(n);
                st.curvature = sg.is_corner() ? 1 / std::abs(sg.radius) : 0;
                st.sign = sg.type == track::SegmentType::Left ? -1.0 : 1.0;
                st.slope = sg.slope;
                st.lap = lap;
                const double vlim = seg_limit[lap][k];
                node_limit.back() = std::min(node_limit.back(), vlim);
                for (std::size_t m = 1; m <= n; ++m) {
                    steps.push_back(st);
                    const double local =
                        m == n ? seg_end : seg_start + sg.length * static_cast<double>(m) /
                                                           static_cast<double>(n);
                    node_dist.push_back(static_cast<double>(lap) * lap_length + local);
                    node_limit.push_back(vlim);
                }
            }
            seg_start = seg_end;
        }
    }
    lap_node[laps] = node_dist.size() - 1;
    const std::size_t n_steps = steps.size();
    if (n_steps == 0) throw std::invalid_argument("pista sem comprimento");

    // --- Passada para trás: envelope de frenagem ---
    std::vector<double> env(n_steps + 1);
    env[n_steps] = node_limit[n_steps];
    for (std::size_t i = n_steps; i-- > 0;) {
        const double v1 = env[i + 1];
        double vb = kInf;
        if (std::isfinite(v1)) {
            const Step& st = steps[i];
            const Car& car = cars[st.lap];
            const double d1 = qs::max_braking(car, v1, st.curvature, st.slope).decel;
            // Corretor avaliado no máximo no limite do ponto (acima dele a curva não fecha).
            const double vp = std::min(node_limit[i],
                                       std::sqrt(std::max(0.0, v1 * v1 + 2 * d1 * st.ds)));
            const double d2 = qs::max_braking(car, vp, st.curvature, st.slope).decel;
            vb = std::sqrt(std::max(0.0, v1 * v1 + (d1 + d2) * st.ds));
        }
        env[i] = std::min(node_limit[i], vb);
    }

    // --- Passada para frente ---
    Telemetry& h = r.history;
    h.reserve(n_steps + 1);
    double v = std::min(options.initial_velocity, env[0]);
    if (options.initial_velocity > env[0] + 1e-9) {
        logf(log, "[AVISO] velocidade inicial acima do limite do primeiro trecho: %.2f km/h\n",
             env[0] * 3.6);
    }
    int gear = qs::best_gear(cars[0], v);
    double shift_timer = 0;  // tempo restante da troca [s]
    double t = 0, fuel = 0, temp = options.initial_brake_temp;

    auto record = [&](const Car& car, double vel, double acc, double ay_signed, double slope,
                      double f_engine, double f_brake, const physics::BrakeTempResult& bt) {
        const double down = physics::downforce(car, vel);
        const qs::AxleState s = qs::axle_state(car, vel, acc, std::abs(ay_signed), slope);
        const auto& n = s.normal;
        h.time.push_back(t);
        h.distance.push_back(node_dist[h.time.size() - 1]);
        h.velocity.push_back(vel);
        h.gear.push_back(gear);
        h.acceleration.push_back(acc);
        h.rpm.push_back(control::motor_rpm(car, vel, gear));
        h.lateral_accel.push_back(ay_signed);
        h.drag.push_back(physics::drag(car, vel));
        h.downforce.push_back(down);
        h.rolling_resistance.push_back(physics::tyre_resistance(car, vel, slope, down));
        h.slope_force.push_back(physics::slope_force(car, slope));
        h.engine_force.push_back(f_engine);
        h.brake_force.push_back(f_brake);
        h.brake_temp.push_back(temp);
        h.h_coef.push_back(bt.h);
        h.fuel_consumed_g.push_back(fuel);
        h.p_conv.push_back(bt.p_conv);
        h.p_rad.push_back(bt.p_rad);
        h.p_cond.push_back(bt.p_cond);
        h.load_front_dist.push_back((n[0] + n[1]) / (n[0] + n[1] + n[2] + n[3]) * 100);
    };

    {
        const Step& s0 = steps[0];
        const double ay0 = v * v * s0.curvature * s0.sign;
        record(cars[0], v, 0, ay0, s0.slope, 0, 0,
               physics::brake_temperature(cars[0], v, 0, temp, 0, 0.6));
    }

    for (std::size_t i = 0; i < n_steps; ++i) {
        const Step& st = steps[i];
        const Car& car = cars[st.lap];
        if (i > 0 && steps[i - 1].lap != st.lap) {  // começo de volta: troca o carro
            gear = std::min(gear, car.gear_count());
        }

        // Fração do passo em que o motor traciona (durante a troca a força é zero).
        const double dt_guess = st.ds / std::max(v, 0.1);
        const double on = shift_timer > 0 ? std::max(0.0, 1 - shift_timer / dt_guess) : 1.0;

        const double fe1 = on * qs::drive_force(car, v, gear);
        const double a1 = qs::max_acceleration(car, v, st.curvature, st.slope, fe1);
        const double vp = std::min(env[i + 1], std::sqrt(std::max(0.0, v * v + 2 * a1 * st.ds)));
        const double fe2 = on * qs::drive_force(car, vp, gear);
        const double a2 = qs::max_acceleration(car, vp, st.curvature, st.slope, fe2);
        const double v_full = std::sqrt(std::max(0.0, v * v + (a1 + a2) * st.ds));

        const bool limited = v_full > env[i + 1];
        const double v_next = limited ? env[i + 1] : v_full;
        if (v + v_next <= 1e-9) {
            throw std::runtime_error(
                "modelo melhorado: o carro nao consegue avancar (rampa ou aderencia "
                "insuficiente)");
        }
        const double dt = 2 * st.ds / (v + v_next);
        const double acc = (v_next * v_next - v * v) / (2 * st.ds);
        const double v_mid = 0.5 * (v + v_next);

        // Força longitudinal nos pneus no passo: + motor, − freio.
        // Em plena carga (não limitado pelo envelope) não há freio; com a troca ocupando o
        // passo inteiro também não há motor (evita resíduos de arredondamento).
        const double fx = car.mass * acc + qs::resistance(car, v_mid, st.slope);
        const double f_engine = (!limited && on <= 0) ? 0.0 : std::max(0.0, fx);
        const double f_brake = limited ? std::max(0.0, -fx) : 0.0;

        // Combustível: potência do motor = força nas rodas × v / η, com o BSFC na rotação.
        const double rpm_mid = control::motor_rpm(car, v_mid, gear);
        const double p_kw = f_engine * v_mid / car.powertrain_efficiency / 1000;
        fuel += physics::engine_bsfc(car, rpm_mid) * p_kw * dt / 3600;

        double front_fraction = 1 - car.weight_dist_rear_percent / 100;
        if (f_brake > 0) {
            front_fraction = qs::max_braking(car, v_mid, st.curvature, st.slope).front_fraction;
        }
        const auto bt = physics::brake_temperature(car, v_mid, f_brake, temp, dt, front_fraction);
        temp = bt.new_temp;
        t += dt;
        shift_timer = std::max(0.0, shift_timer - dt);

        // Câmbio: sobe no ponto de troca com o motor tracionando; reduz (sem custo de tempo)
        // ao frear ou aliviar.
        if (f_engine > 0 && shift_timer <= 0 && gear < car.gear_count() &&
            physics::engine_rpm(car, v_next, gear) >
                car.shift_rpm[static_cast<std::size_t>(gear - 1)]) {
            ++gear;
            shift_timer = car.shift_time;
        } else if (limited && f_engine <= 0) {
            gear = std::min(gear, qs::best_gear(car, v_next));
        }

        v = v_next;
        record(car, v, acc, v * v * st.curvature * st.sign, st.slope, f_engine, f_brake, bt);
    }

    // --- Resultados ---
    r.total_time = h.time.back();
    r.max_velocity_kmh = *std::max_element(h.velocity.begin(), h.velocity.end()) * 3.6;
    double max_lat = 0;
    for (double a : h.lateral_accel) max_lat = std::max(max_lat, std::abs(a));
    r.max_lat_g = max_lat / 9.81;
    r.max_brake_temp = *std::max_element(h.brake_temp.begin(), h.brake_temp.end());
    r.total_fuel_g = h.fuel_consumed_g.back();

    r.lap_times.assign(laps, kNaN);
    r.lap_fuel_g.assign(laps, kNaN);
    r.lap_start_index.assign(laps, 0);
    r.lap_mass_kg.resize(laps);
    for (std::size_t k = 0; k < laps; ++k) r.lap_mass_kg[k] = cars[k].mass;
    for (std::size_t k = 0; k < laps; ++k) {
        const std::size_t a = lap_node[k], b = lap_node[k + 1];
        r.lap_start_index[k] = a;
        r.lap_times[k] = h.time[b] - h.time[a];
        r.lap_fuel_g[k] = h.fuel_consumed_g[b] - h.fuel_consumed_g[a];
    }
    if (laps == 1) {
        r.fastest_lap = r.total_time;
    } else {
        r.fastest_lap = *std::min_element(r.lap_times.begin() + 1, r.lap_times.end());
    }

    logf(log, "\n=======================================================\n");
    logf(log, " MODELO MELHORADO (quase-estatico, passo %.3f m): %zu pontos\n", options.step_m,
         n_steps + 1);
    logf(log, "=======================================================\n");
    for (std::size_t k = 0; k < laps; ++k) {
        logf(log, " Volta %2zu: %8.3f s | %7.1f g de combustivel\n", k + 1, r.lap_times[k],
             r.lap_fuel_g[k]);
    }
    logf(log, " Tempo total: %.3f s | Velocidade maxima: %.1f km/h\n", r.total_time,
         r.max_velocity_kmh);
    return r;
}

}  // namespace fsae::sim
