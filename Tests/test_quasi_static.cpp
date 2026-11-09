// Modelo melhorado (quase-estático em distância, Simulation/quasi_static.hpp).
// Os testes conferem casos com solução analítica, o respeito à elipse de atrito e o sentido
// físico das variações (mais downforce → mais rápido, mais massa → mais lento etc.).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "IO_Tools/car_json.hpp"
#include "Physics/powertrain.hpp"
#include "Physics/resistances.hpp"
#include "Simulation/lap.hpp"
#include "Simulation/quasi_static.hpp"
#include "Tracks/corner_speed.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"

using fsae::Car;
using fsae::CarSpec;
using fsae::ModelOptions;
namespace sim = fsae::sim;
namespace qs = fsae::sim::qs;
namespace tr = fsae::track;
namespace ph = fsae::physics;

namespace {

/// Carro padrão com as correções ligadas (como o modelo melhorado usa internamente).
Car base_car() {
    Car c = Car::matlab_default();
    c.model = ModelOptions::corrected();
    return c;
}

/// Sem aerodinâmica (solução analítica simples).
Car no_aero_car() {
    Car c = base_car();
    c.cl = 0;
    c.cd = 0;
    return c;
}

tr::Track builtin(const std::string& name) {
    return tr::load_track(std::string(FSAE_DATA_DIR) + "/Tracks/" + name + ".csv");
}

sim::SimulationResult run_qs(const Car& car, const std::vector<tr::Segment>& seg, int laps = 1,
                             double v0 = 0, double step = 0.25) {
    sim::SimulationOptions o;
    o.laps = laps;
    o.initial_velocity = v0;
    o.solver = sim::Solver::QuasiStatic;
    o.step_m = step;
    return sim::simulate(car, seg, o);
}

tr::Segment straight(double L, double slope = 0) {
    tr::Segment s;
    s.length = L;
    s.slope = slope;
    return s;
}

tr::Segment corner(double L, double R, tr::SegmentType type = tr::SegmentType::Right) {
    tr::Segment s;
    s.length = L;
    s.radius = R;
    s.type = type;
    return s;
}

double rolling(const Car& c, double v) { return ph::tyre_resistance(c, v, 0, ph::downforce(c, v)); }

/// Maior uso da elipse na telemetria (pista plana).
double max_usage(const Car& car, const sim::SimulationResult& r) {
    const auto& h = r.history;
    double u = 0;
    for (std::size_t i = 0; i < h.size(); ++i) {
        u = std::max(u, qs::friction_usage(car, h.velocity[i], h.acceleration[i],
                                           h.lateral_accel[i], 0, h.engine_force[i],
                                           h.brake_force[i]));
    }
    return u;
}

}  // namespace

TEST_CASE("curva sem aerodinamica: v = sqrt(mu*g*R)") {
    const Car c = no_aero_car();
    for (double R : {5.0, 10.0, 25.0, 60.0}) {
        CAPTURE(R);
        const auto lim = qs::corner_limit(c, R, 0);
        const double analytic = std::sqrt(c.muy * c.g * R);
        // A traseira ainda precisa vencer o rolamento dentro da elipse: um pouco abaixo.
        CHECK(lim.v_max <= analytic);
        CHECK(lim.v_max > 0.995 * analytic);
        CHECK_FALSE(lim.limited_by_tipping());
    }
}

TEST_CASE("curva com downforce: igual a solucao analitica do modelo original") {
    // Com peso e downforce divididos 50/50, a dianteira e a traseira chegam juntas ao limite
    // lateral, como na formula fechada m*v²/R = muy*(m*g + K*v²). Sem arrasto, so o
    // rolamento separa as duas.
    Car c = base_car();
    c.cd = 0;
    for (double R : {8.0, 20.0, 50.0}) {
        CAPTURE(R);
        const auto ref = tr::max_safe_corner_speed(c, R, 0);
        const auto lim = qs::corner_limit(c, R, 0);
        CHECK(lim.v_max <= ref.v_grip + 1e-9);
        CHECK(lim.v_max > 0.998 * ref.v_grip);
    }
}

TEST_CASE("curva com arrasto: a traseira tambem empurra contra o arrasto (elipse)") {
    const Car c = base_car();
    Car no_drag = c;
    no_drag.cd = 0;
    double prev_loss = 0;
    for (double R : {8.0, 20.0, 50.0}) {
        CAPTURE(R);
        const double with_drag = qs::corner_limit(c, R, 0).v_max;
        const double without = qs::corner_limit(no_drag, R, 0).v_max;
        const double loss = 1 - with_drag / without;
        CHECK(loss > 0);
        CHECK(loss < 0.02);
        CHECK(loss > prev_loss);  // curva mais rapida, mais arrasto, mais perda
        prev_loss = loss;
    }
    // R = 100 m: a formula fechada da 66 m/s, mas o carro nao passa de ~37 m/s: sem limite.
    // Acima de R = m/(muy*K) nem a formula limita (downforce cresce mais que a centripeta).
    CHECK(tr::max_safe_corner_speed(c, 100, 0).v_grip > ph::max_vehicle_speed(c));
    CHECK(std::isinf(qs::corner_limit(c, 100, 0).v_max));
    CHECK(std::isinf(qs::corner_limit(c, 200, 0).v_max));
}

TEST_CASE("curva limitada por tombamento com CG alto") {
    Car c = base_car();
    c.cg_height = 0.9;
    const auto lim = qs::corner_limit(c, 15, 0);
    CHECK(lim.limited_by_tipping());
    // No limite, a roda interna fica descarregada
    const auto s = qs::axle_state(c, lim.v_tip, 0, lim.v_tip * lim.v_tip / 15, 0);
    const double min_load = *std::min_element(s.normal.begin(), s.normal.end());
    CHECK(min_load == doctest::Approx(0.01).epsilon(1e-3));
}

TEST_CASE("frenagem em reta sem aerodinamica: d = mux*g + rolamento/m") {
    const Car c = no_aero_car();
    for (double v : {5.0, 15.0, 30.0}) {
        CAPTURE(v);
        const auto b = qs::max_braking(c, v, 0, 0);
        CHECK(b.decel == doctest::Approx(c.mux_static * c.g + rolling(c, v) / c.mass).epsilon(1e-9));
        // Distribuicao ideal: a dianteira recebe mais que o peso estatico (transferencia)
        const double h_l = c.cg_height / c.wheelbase;
        const double expected_front = 0.5 + h_l * (b.decel / c.g);
        CHECK(b.front_fraction == doctest::Approx(expected_front).epsilon(1e-9));
    }
}

TEST_CASE("balanco de freio fixo: so iguala o ideal no ponto certo") {
    Car c = no_aero_car();
    const auto ideal = qs::max_braking(c, 20, 0, 0);
    // Com mux = 2,1 e h/L = 0,2 a transferencia e grande: o ideal fica perto de 93% na frente
    CHECK(ideal.front_fraction > 0.9);
    c.brake_bias_front = ideal.front_fraction;
    CHECK(qs::max_braking(c, 20, 0, 0).decel == doctest::Approx(ideal.decel).epsilon(1e-6));
    double prev = 0;
    for (double offset : {-0.4, -0.2, -0.05}) {  // cada vez mais perto do ideal
        const double bias = ideal.front_fraction + offset;
        CAPTURE(bias);
        c.brake_bias_front = bias;
        const auto b = qs::max_braking(c, 20, 0, 0);
        CHECK(b.decel < ideal.decel);
        CHECK(b.decel > prev);
        CHECK(b.front_fraction == bias);
        prev = b.decel;
    }
    // Acima do ideal: a dianteira trava antes
    c.brake_bias_front = std::min(0.99, ideal.front_fraction + 0.05);
    CHECK(qs::max_braking(c, 20, 0, 0).decel < ideal.decel);
}

TEST_CASE("torque maximo de freio limita a desaceleracao") {
    Car c = no_aero_car();
    c.brake_max_torque = 400;  // N·m: bem abaixo da aderencia
    const double v = 20;
    const auto b = qs::max_braking(c, v, 0, 0);
    CHECK(b.brake_force == doctest::Approx(400 / c.tire_radius));
    CHECK(b.decel == doctest::Approx((400 / c.tire_radius + rolling(c, v)) / c.mass).epsilon(1e-9));
}

TEST_CASE("tracao limitada pela aderencia traseira com transferencia de carga") {
    const Car c = no_aero_car();
    // Motor muito forte: a = (mux*g*w - res/m) / (1 - mux*h/L)
    const double v = 1.0;
    const double a = qs::max_acceleration(c, v, 0, 0, 1e6);
    const double w = c.weight_dist_rear_percent / 100;
    const double expected = (c.mux_static * c.g * w - rolling(c, v) / c.mass) /
                            (1 - c.mux_static * c.cg_height / c.wheelbase);
    CHECK(a == doctest::Approx(expected).epsilon(1e-9));
    // Motor fraco: limitado pelo motor
    const double fe = 1000;
    CHECK(qs::max_acceleration(c, v, 0, 0, fe) ==
          doctest::Approx((fe - rolling(c, v)) / c.mass).epsilon(1e-12));
}

TEST_CASE("elipse de atrito: curva reduz frenagem e aceleracao") {
    const Car c = base_car();
    const double R = 30;
    const double v_lim = qs::corner_limit(c, R, 0).v_max;
    const double fe = 1e6;
    double prev_brake = qs::max_braking(c, 0.5 * v_lim, 0, 0).decel;
    // Mesma velocidade, curvatura crescente: menos sobra para frear e acelerar
    for (double frac : {0.5, 0.8, 0.95}) {
        const double v = frac * v_lim;
        CAPTURE(frac);
        const double straight_b = qs::max_braking(c, v, 0, 0).decel;
        const double corner_b = qs::max_braking(c, v, 1 / R, 0).decel;
        CHECK(corner_b < straight_b);
        CHECK(qs::max_acceleration(c, v, 1 / R, 0, fe) < qs::max_acceleration(c, v, 0, 0, fe));
        (void)prev_brake;
    }
    // No limite da curva nao sobra quase nada para frear
    CHECK(qs::max_braking(c, v_lim, 1 / R, 0).decel < 0.5);
}

TEST_CASE("sensibilidade a carga: k > 0 reduz a aderencia de quem recebe carga") {
    Car c = base_car();
    const double v_k0 = qs::corner_limit(c, 20, 0).v_max;
    const double b_k0 = qs::max_braking(c, 20, 0, 0).decel;
    c.tyre_load_sensitivity = 0.15;
    CHECK(qs::corner_limit(c, 20, 0).v_max < v_k0);
    CHECK(qs::max_braking(c, 20, 0, 0).decel < b_k0);
    // Na carga estatica o atrito nao muda
    const auto s = qs::axle_state(c, 0, 0, 0, 0);
    CHECK(s.cap_x_front == doctest::Approx(c.mux_static * c.mass * c.g / 2));
}

TEST_CASE("velocidade final numa reta longa: equilibrio de forcas ou limitador") {
    const Car c = base_car();
    const auto r = run_qs(c, {straight(3000)});
    const auto& h = r.history;
    const double v_end = h.velocity.back();
    CHECK(std::abs(h.acceleration.back()) < 0.05);
    CHECK(h.rpm.back() <= c.engine.rpm_max() + 1);
    CHECK(h.gear.back() == c.gear_count());
    // Velocidade maxima da transmissao no limitador
    CHECK(v_end <= ph::max_vehicle_speed(c) + 1e-6);
    CHECK(v_end > 0.95 * ph::max_vehicle_speed(c));
}

TEST_CASE("prova de aceleracao: proxima do modelo original (largada limitada pelo motor)") {
    const auto seg = builtin("acceleration").segments;
    sim::SimulationOptions o;
    const double t_orig = sim::simulate(Car::matlab_default(), seg, o).lap_times[0];
    const double t_qs = run_qs(Car::matlab_default(), seg).lap_times[0];
    CHECK(std::abs(t_qs - t_orig) / t_orig < 0.01);
}

TEST_CASE("acelera dentro da curva ate o limite, sem passar") {
    const Car c = base_car();
    const double R = 30;
    const double v_lim = qs::corner_limit(c, R, 0).v_max;
    const auto r = run_qs(c, {straight(1), corner(200, R)}, 1, 8.0);
    const auto& h = r.history;
    // A velocidade so cai durante a troca de marcha (sem motor e sem freio, so o arrasto)
    bool only_while_shifting = true;
    int shifts = 0;
    for (std::size_t i = 1; i < h.size(); ++i) {
        if (h.gear[i] != h.gear[i - 1]) ++shifts;
        if (h.velocity[i] < h.velocity[i - 1] - 1e-9) {
            only_while_shifting = only_while_shifting && h.engine_force[i] == 0 &&
                                  h.brake_force[i] == 0;
        }
        CHECK(h.velocity[i] <= v_lim + 1e-9);
    }
    CHECK(only_while_shifting);
    CHECK(shifts >= 1);
    CHECK(h.velocity.back() > 0.999 * v_lim);
    CHECK(h.velocity.back() > h.velocity.front() + 10);
}

TEST_CASE("curva rapida seguida de curva lenta: freia antes, sem queda instantanea") {
    const Car c = base_car();
    const double v_fast = qs::corner_limit(c, 60, 0).v_max;
    const double v_slow = qs::corner_limit(c, 10, 0).v_max;
    const auto r = run_qs(c, {corner(80, 60), corner(30, 10, tr::SegmentType::Left)}, 1, v_fast);
    const auto& h = r.history;
    double max_jump = 0;
    std::size_t entry = 0;
    for (std::size_t i = 1; i < h.size(); ++i) {
        max_jump = std::max(max_jump, h.velocity[i - 1] - h.velocity[i]);
        if (h.distance[i] <= 80 + 1e-9) entry = i;
    }
    CHECK(max_jump < 1.0);  // passos de 0,25 m: nenhuma queda brusca
    CHECK(h.velocity[entry] <= v_slow + 1e-9);
    // Freou ao longo de varios metros dentro da curva rapida
    std::size_t braking = 0;
    for (std::size_t i = 0; i <= entry; ++i) braking += h.brake_force[i] > 0 ? 1 : 0;
    CHECK(braking * 0.25 > 5.0);
}

TEST_CASE("pistas: elipse respeitada, distancia exata, voltas lancadas iguais") {
    const Car c = Car::matlab_default();
    for (const char* name : {"autox", "skidpad", "acceleration"}) {
        CAPTURE(name);
        const auto t = builtin(name);
        const int laps = std::string(name) == "autox" ? 5 : 1;
        const auto r = run_qs(c, t.segments, laps);
        // Uso da elipse: so o erro de amostragem do passo (forca media do passo no fim dele)
        CHECK(max_usage(base_car(), r) < 1.02);
        CHECK(r.history.distance.back() == doctest::Approx(laps * r.track_length).epsilon(1e-12));
        double rpm_max = *std::max_element(r.history.rpm.begin(), r.history.rpm.end());
        CHECK(rpm_max <= c.engine.rpm_max());
        if (laps > 1) {
            for (int k = 2; k < laps; ++k) {
                CHECK(r.lap_times[static_cast<std::size_t>(k)] ==
                      doctest::Approx(r.lap_times[1]).epsilon(1e-6));
            }
            CHECK(r.lap_times[0] > r.lap_times[1]);  // a 1ª volta parte parada
            CHECK(r.fastest_lap == doctest::Approx(r.lap_times[1]));
        }
    }
}

TEST_CASE("passo de distancia: resultado converge") {
    const auto seg = builtin("autox").segments;
    const double coarse = run_qs(Car::matlab_default(), seg, 1, 0, 0.5).lap_times[0];
    const double base = run_qs(Car::matlab_default(), seg, 1, 0, 0.25).lap_times[0];
    const double fine = run_qs(Car::matlab_default(), seg, 1, 0, 0.05).lap_times[0];
    CHECK(std::abs(base - fine) / fine < 5e-4);
    CHECK(std::abs(coarse - fine) > std::abs(base - fine) - 1e-6);
    // Elipse converge para 1 com passo menor
    const auto r = run_qs(base_car(), seg, 1, 0, 0.05);
    CHECK(max_usage(base_car(), r) < 1.005);
}

TEST_CASE("sentido fisico das variacoes (autocross)") {
    const auto seg = builtin("autox").segments;
    const Car c = Car::matlab_default();
    const double t0 = run_qs(c, seg).lap_times[0];

    Car more_down = c;
    more_down.cl *= 1.2;
    CHECK(run_qs(more_down, seg).lap_times[0] < t0);

    Car heavier = c;
    heavier.mass += 20;
    CHECK(run_qs(heavier, seg).lap_times[0] > t0);

    Car grippier = c;
    grippier.muy *= 1.1;
    CHECK(run_qs(grippier, seg).lap_times[0] < t0);

    Car sensitive = c;
    sensitive.tyre_load_sensitivity = 0.15;
    CHECK(run_qs(sensitive, seg).lap_times[0] > t0);

    Car biased = c;
    biased.brake_bias_front = 0.70;  // balanco fixo nunca supera a distribuicao ideal
    CHECK(run_qs(biased, seg).lap_times[0] > t0);
}

TEST_CASE("rampa: subida mais lenta, descida mais rapida") {
    const Car c = Car::matlab_default();
    const double flat = run_qs(c, {straight(150)}).lap_times[0];
    CHECK(run_qs(c, {straight(150, 0.05)}).lap_times[0] > flat);
    CHECK(run_qs(c, {straight(150, -0.05)}).lap_times[0] < flat);
}

TEST_CASE("modelo melhorado nao muda o original") {
    const auto seg = builtin("autox").segments;
    sim::SimulationOptions o;
    o.laps = 2;
    const auto r = sim::simulate(Car::matlab_default(), seg, o);
    CHECK(r.lap_times[1] == doctest::Approx(45.5006).epsilon(1e-5));
    CHECK(sim::parse_solver("melhorado") == sim::Solver::QuasiStatic);
    CHECK(sim::parse_solver("original") == sim::Solver::Original);
    CHECK_THROWS(sim::parse_solver("xyz"));
}

TEST_CASE("JSON: campos novos sao opcionais e fazem ida e volta") {
    CarSpec spec = CarSpec::matlab_default();
    // Sem os campos novos (arquivo antigo): valores padrao
    auto j = nlohmann::json::parse(fsae::io::car_spec_to_json(spec));
    j["tyre"].erase("load_sensitivity");
    j["brakes"].erase("max_torque_nm");
    j["brakes"].erase("bias_front_percent");
    const CarSpec old = fsae::io::parse_car_spec(j.dump());
    CHECK(old.tyre_load_sensitivity == 0);
    CHECK(old.brake_max_torque_nm == 3000);
    CHECK_FALSE(old.brake_bias_front_percent.has_value());

    spec.tyre_load_sensitivity = 0.12;
    spec.brake_max_torque_nm = 2500;
    spec.brake_bias_front_percent = 62.5;
    const CarSpec back = fsae::io::parse_car_spec(fsae::io::car_spec_to_json(spec));
    CHECK(back.tyre_load_sensitivity == 0.12);
    CHECK(back.brake_max_torque_nm == 2500);
    REQUIRE(back.brake_bias_front_percent.has_value());
    CHECK(*back.brake_bias_front_percent == 62.5);
    const Car car = Car::from_spec(back);
    REQUIRE(car.brake_bias_front.has_value());
    CHECK(*car.brake_bias_front == doctest::Approx(0.625));

    spec.brake_bias_front_percent = 120;
    CHECK_THROWS(Car::from_spec(spec));
    spec.brake_bias_front_percent.reset();
    spec.tyre_load_sensitivity = 1.5;
    CHECK_THROWS(Car::from_spec(spec));
}
