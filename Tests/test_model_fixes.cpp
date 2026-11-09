// Correções pontuais do modelo (Fase A, ver Vehicle/model_options.hpp).
// Cada teste confere que a correção faz o que promete e que o efeito tem o sentido físico
// esperado. Com tudo desligado, os demais testes garantem o modelo original intacto.
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Physics/powertrain.hpp"
#include "Simulation/lap.hpp"
#include "Simulation/straight.hpp"
#include "Tracks/corner_speed.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"

using fsae::Car;
using fsae::ModelOptions;
namespace sim = fsae::sim;
namespace tr = fsae::track;
namespace ph = fsae::physics;

namespace {

Car car_with(ModelOptions m) {
    Car c = Car::matlab_default();
    c.model = m;
    return c;
}

tr::Track builtin(const std::string& name) {
    return tr::load_track(std::string(FSAE_DATA_DIR) + "/Tracks/" + name + ".csv");
}

sim::SimulationResult run(const std::vector<tr::Segment>& seg, int laps, ModelOptions m) {
    sim::SimulationOptions o;
    o.laps = laps;
    o.model = m;
    return sim::simulate(Car::matlab_default(), seg, o);
}

ModelOptions only(bool ModelOptions::*flag) {
    ModelOptions m;
    m.*flag = true;
    return m;
}

}  // namespace

TEST_CASE("1. inclinacao em radianos na velocidade de curva") {
    const double theta = 0.13;  // 7,4 graus
    const double R = 20.0;      // curva limitada por aderencia (nao por tombamento)
    const Car on = car_with(only(&ModelOptions::slope_in_radians));
    const Car off = car_with(ModelOptions::original());

    const auto flat = tr::max_safe_corner_speed(on, R, 0.0);
    const auto slope = tr::max_safe_corner_speed(on, R, theta);
    REQUIRE_FALSE(flat.limited_by_tipping());
    // v² = μ·m·g·cosθ·R / (m − μ·K·R): a rampa so muda o termo do peso
    CHECK(slope.v_max / flat.v_max == doctest::Approx(std::sqrt(std::cos(theta))).epsilon(1e-12));

    const std::vector<tr::Segment> seg = {{30, R, theta, tr::SegmentType::Left}};
    CHECK(tr::corner_speeds(on, seg)[0].v_max == slope.v_max);
    // Original: atan(slope/100), praticamente plano
    CHECK(tr::corner_speeds(off, seg)[0].v_max ==
          tr::max_safe_corner_speed(off, R, std::atan(theta / 100)).v_max);
    CHECK(tr::corner_speeds(on, seg)[0].v_max < tr::corner_speeds(off, seg)[0].v_max);
}

TEST_CASE("2. curva de raio grande deixa de ter velocidade zero") {
    const Car on = car_with(only(&ModelOptions::unbounded_corner_grip));
    const Car off = car_with(ModelOptions::original());
    // Raio-limite: m = μy·K·R
    const double K = 0.5 * on.rho * on.cl * on.frontal_area;
    const double R_lim = on.mass / (on.muy * K);
    CHECK(R_lim == doctest::Approx(136.5).epsilon(1e-3));

    CHECK(tr::max_safe_corner_speed(off, 200.0, 0).v_max == 0.0);
    CHECK(std::isinf(tr::max_safe_corner_speed(on, 200.0, 0).v_max));
    // Abaixo do limite nada muda
    CHECK(tr::max_safe_corner_speed(on, 100.0, 0).v_max == tr::max_safe_corner_speed(off, 100.0, 0).v_max);

    // Pista com uma curva de 200 m: o original "para" na curva (0,1 m/s); corrigido, nao
    const std::vector<tr::Segment> seg = {{50, 0, 0, tr::SegmentType::Straight},
                                          {100, 200, 0, tr::SegmentType::Left},
                                          {50, 0, 0, tr::SegmentType::Straight}};
    const auto a = run(seg, 1, ModelOptions::original());
    const auto b = run(seg, 1, only(&ModelOptions::unbounded_corner_grip));
    CHECK(a.total_time > 900.0);  // 100 m a 0,1 m/s
    CHECK(b.total_time < 15.0);
    // Sem limite de aderencia, o carro passa na curva com a velocidade que trouxe da reta
    const auto& h = b.history;
    double v_corner = 0;
    for (std::size_t i = 0; i < h.size(); ++i) {
        if (h.distance[i] > 60 && h.distance[i] < 140) v_corner = h.velocity[i];
    }
    CHECK(v_corner > 15.0);
}

TEST_CASE("3. reta termina exatamente no fim e voltas somam a distancia exata") {
    // Reta de 75 m acelerando: o original passa do fim; corrigido, termina em 75 m
    const Car off = car_with(ModelOptions::original());
    const Car on = car_with(only(&ModelOptions::exact_segment_end));
    const auto a = sim::simulate_straight(off, 75.0, 0.0, sim::kNoTarget, 0.0);
    const auto b = sim::simulate_straight(on, 75.0, 0.0, sim::kNoTarget, 0.0);
    CHECK(a.history.position.back() > 75.0);
    CHECK(b.history.position.back() == 75.0);
    CHECK(b.total_time <= a.total_time);
    CHECK(a.total_time - b.total_time < sim::kStraightDt);  // no maximo um passo

    // Autocross em 2 e 25 voltas: distancia final = voltas × comprimento
    for (int laps : {2, 25}) {
        const auto t = builtin("autox");
        const auto r = run(t.segments, laps, only(&ModelOptions::exact_segment_end));
        CAPTURE(laps);
        CHECK(r.history.distance.back() == doctest::Approx(laps * r.track_length).epsilon(1e-12));
        double sum = 0;
        for (double l : r.lap_times) sum += l;
        CHECK(sum == doctest::Approx(r.total_time).epsilon(1e-12));
    }

    // Skidpad: o original freava um passo inteiro (0,01 s a ~22 m/s²) para cobrir os
    // ultimos 0,8 mm da reta e entrava na curva mais devagar. Corrigido, entra mais rapido,
    // sem passar do limite da curva.
    const auto sk = builtin("skidpad");
    const auto ro = run(sk.segments, 1, ModelOptions::original());
    const auto rc = run(sk.segments, 1, only(&ModelOptions::exact_segment_end));
    const double v_limit = rc.corner_speeds[1].v_max;
    const double v_orig = ro.history.velocity.back();
    const double v_corr = rc.history.velocity.back();
    CHECK(v_corr > v_orig);
    CHECK(v_corr <= v_limit);
    CHECK(rc.total_time < ro.total_time);
}

TEST_CASE("4. reducao de marcha sem sobregiro") {
    for (const char* name : {"autox", "autox_2024_v1", "autox_2024_v1_reversed"}) {
        CAPTURE(name);
        const auto t = builtin(name);
        const auto a = run(t.segments, 2, ModelOptions::original());
        const auto b = run(t.segments, 2, only(&ModelOptions::downshift_rev_check));
        const double max_a = *std::max_element(a.history.rpm.begin(), a.history.rpm.end());
        const double max_b = *std::max_element(b.history.rpm.begin(), b.history.rpm.end());
        const Car car = Car::matlab_default();
        const double shift = *std::max_element(car.shift_rpm.begin(), car.shift_rpm.end());
        CHECK(max_a > car.engine.rpm_max());  // o original passa do limite do motor
        // Corrigido: no maximo o rpm de troca mais o que sobe em um passo de 0,01 s
        CHECK(max_b <= shift + 50);
        // Durante a frenagem a forca do motor nao e usada: os tempos nao mudam
        CHECK(b.lap_times[0] == a.lap_times[0]);
        CHECK(b.lap_times[1] == a.lap_times[1]);
    }
}

TEST_CASE("5. mapas do motor limitados a faixa medida") {
    const Car on = car_with(only(&ModelOptions::limit_engine_map));
    const Car off = car_with(ModelOptions::original());
    const double lo = on.engine.rpm_min(), hi = on.engine.rpm_max();
    // Fora da faixa: o original extrapola valores absurdos
    CHECK(ph::engine_torque(off, 14000) > 1000.0);
    CHECK(ph::engine_torque(on, 14000) == 0.0);          // limitador corta
    CHECK(ph::engine_torque(on, 3000) == on.engine.torque(lo));
    // Dentro da faixa, identico
    for (double rpm : {lo, 8000.0, 11000.0, hi}) CHECK(ph::engine_torque(on, rpm) == ph::engine_torque(off, rpm));
    const auto& bx = on.engine.bsfc_rpm();
    CHECK(ph::engine_bsfc(on, bx.front() - 2000) == on.engine.bsfc(bx.front()));
    CHECK(ph::engine_bsfc(on, bx.back() + 2000) == on.engine.bsfc(bx.back()));

    // Nas pistas o motor fica dentro da faixa quando acelera: tempos iguais
    const auto t = builtin("autox");
    const auto a = run(t.segments, 2, ModelOptions::original());
    const auto b = run(t.segments, 2, only(&ModelOptions::limit_engine_map));
    CHECK(b.lap_times[0] == a.lap_times[0]);
    CHECK(b.lap_times[1] == a.lap_times[1]);
}

TEST_CASE("todas as correcoes juntas: coerencia geral") {
    for (const char* name : {"autox", "autox_2024_v1", "autox_2024_v1_reversed", "skidpad", "acceleration"}) {
        CAPTURE(name);
        const auto t = builtin(name);
        const int laps = std::string(name) == "skidpad" || std::string(name) == "acceleration" ? 1 : 2;
        const auto a = run(t.segments, laps, ModelOptions::original());
        const auto b = run(t.segments, laps, ModelOptions::corrected());
        // Diferencas pequenas: correcoes de detalhe, nao mudanca de modelo. A maior e a do
        // skidpad (2 %), que vem do passo de frenagem artificial no fim da reta (teste 3).
        CHECK(std::abs(b.total_time - a.total_time) / a.total_time < 0.03);
        CHECK(b.history.distance.back() == doctest::Approx(laps * b.track_length).epsilon(1e-12));
        const Car car = Car::matlab_default();
        CHECK(*std::max_element(b.history.rpm.begin(), b.history.rpm.end()) <= car.engine.rpm_max());
    }
}
