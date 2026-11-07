#include <doctest/doctest.h>

#include <vector>

#include "Physics/longitudinal.hpp"
#include "Physics/resistances.hpp"
#include "Vehicle/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::control::ShiftState;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
namespace ph = fsae::physics;

namespace {

constexpr double kDt = 0.01;  // mesmo dt do export_reference.m

/// sin/cos (rampa) podem divergir no último bit em outras libm; ver test_physics.cpp.
constexpr double kLibm = 1e-15;

const Car& car() {
    static const Car c = Car::matlab_default();
    return c;
}

}  // namespace

TEST_CASE("forcas, cambio e torque de largada (calculate_forces.csv)") {
    const auto ref = load_reference("unit/calculate_forces.csv");
    const auto& v = ref.column("v");
    const auto& s = ref.column("slope");
    const auto& g = ref.column("gear");
    const auto& acc = ref.column("is_accelerating");
    const auto& shifting = ref.column("is_shifting");
    const auto& timer = ref.column("timer");

    std::vector<double> fe, fd, fr, fs, ng, nsh, nt, fdown;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        int gear = static_cast<int>(g[i]);
        ShiftState st{shifting[i] != 0.0, timer[i]};
        const auto f = ph::calculate_forces(car(), v[i], s[i], gear, acc[i] != 0.0, st, kDt);
        fe.push_back(f.engine_force);
        fd.push_back(f.drag);
        fr.push_back(f.rolling_resistance);
        fs.push_back(f.slope_force);
        fdown.push_back(f.downforce);
        ng.push_back(gear);
        nsh.push_back(st.is_shifting ? 1.0 : 0.0);
        nt.push_back(st.timer);
    }
    check_against_reference("engine_force", ref.column("engine_force"), fe, 0.0);
    check_against_reference("drag", ref.column("drag"), fd, 0.0);
    check_against_reference("rolling_resistance", ref.column("rolling_resistance"), fr, kLibm);
    check_against_reference("slope_force", ref.column("slope_force"), fs, kLibm);
    check_against_reference("downforce", ref.column("downforce"), fdown, 0.0);
    check_against_reference("new_gear", ref.column("new_gear"), ng, 0.0);
    check_against_reference("new_is_shifting", ref.column("new_is_shifting"), nsh, 0.0);
    check_against_reference("new_timer", ref.column("new_timer"), nt, 0.0);
}

TEST_CASE("passo acelerando (car_accelerate.csv)") {
    const auto ref = load_reference("unit/car_accelerate.csv");
    const auto& v = ref.column("v");
    const auto& fe = ref.column("engine_force");
    const auto& s = ref.column("slope");
    std::vector<double> pos, vel, a;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        const double down = ph::downforce(car(), v[i]);
        const double drag = ph::drag(car(), v[i]);
        const double roll = ph::tyre_resistance(car(), v[i], s[i], down);
        const double sf = ph::slope_force(car(), s[i]);
        const auto r = ph::car_accelerate(car(), fe[i], drag, roll, sf, v[i], 0.0, kDt, s[i]);
        pos.push_back(r.position);
        vel.push_back(r.velocity);
        a.push_back(r.acceleration);
    }
    check_against_reference("pos", ref.column("pos"), pos, kLibm);
    check_against_reference("vel", ref.column("vel"), vel, kLibm);
    check_against_reference("acc", ref.column("acc"), a, kLibm);
}

TEST_CASE("passo freando (car_brake.csv)") {
    const auto ref = load_reference("unit/car_brake.csv");
    const auto& v = ref.column("v");
    const auto& fb = ref.column("brake_force");
    const auto& s = ref.column("slope");
    std::vector<double> pos, vel, a, actual;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        const double down = ph::downforce(car(), v[i]);
        const double drag = ph::drag(car(), v[i]);
        const double roll = ph::tyre_resistance(car(), v[i], s[i], down);
        const double sf = ph::slope_force(car(), s[i]);
        const auto r = ph::car_brake(car(), fb[i], drag, roll, sf, v[i], 0.0, kDt, s[i]);
        pos.push_back(r.position);
        vel.push_back(r.velocity);
        a.push_back(r.acceleration);
        actual.push_back(r.actual_brake_force);
    }
    check_against_reference("pos", ref.column("pos"), pos, kLibm);
    check_against_reference("vel", ref.column("vel"), vel, kLibm);
    check_against_reference("acc", ref.column("acc"), a, kLibm);
    check_against_reference("actual_brake_force", ref.column("actual_brake_force"), actual, kLibm);
}
