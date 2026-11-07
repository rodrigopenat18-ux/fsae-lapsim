#include <doctest/doctest.h>

#include <vector>

#include "Control/gearbox.hpp"
#include "Vehicle/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::control::ShiftState;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
namespace ctl = fsae::control;

namespace {

const Car& car() {
    static const Car c = Car::matlab_default();
    return c;
}

}  // namespace

TEST_CASE("marcha inicial (initial_gear.csv)") {
    const auto ref = load_reference("unit/initial_gear.csv");
    std::vector<double> gear;
    for (double v : ref.column("v")) gear.push_back(ctl::find_initial_gear(car(), v));
    check_against_reference("gear", ref.column("gear"), gear, 0.0);
}

TEST_CASE("gerenciamento do cambio (manage_gear.csv)") {
    const auto ref = load_reference("unit/manage_gear.csv");
    constexpr double kDt = 0.01;  // mesmo dt do export_reference.m
    const auto& v = ref.column("v");
    const auto& g = ref.column("gear");
    const auto& acc = ref.column("is_accelerating");
    const auto& shifting = ref.column("is_shifting");
    const auto& timer = ref.column("timer");

    std::vector<double> new_gear, new_shifting, new_timer;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        ShiftState st{shifting[i] != 0.0, timer[i]};
        const int out = ctl::manage_gear(car(), v[i], static_cast<int>(g[i]), acc[i] != 0.0, st, kDt);
        new_gear.push_back(out);
        new_shifting.push_back(st.is_shifting ? 1.0 : 0.0);
        new_timer.push_back(st.timer);
    }
    check_against_reference("new_gear", ref.column("new_gear"), new_gear, 0.0);
    check_against_reference("new_is_shifting", ref.column("new_is_shifting"), new_shifting, 0.0);
    check_against_reference("new_timer", ref.column("new_timer"), new_timer, 0.0);
}

TEST_CASE("rotacao do motor com two-step (powertrain.csv, motor_rpm)") {
    const auto ref = load_reference("unit/powertrain.csv");
    const auto& v = ref.column("v");
    const auto& g = ref.column("gear");
    std::vector<double> rpm;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        rpm.push_back(ctl::motor_rpm(car(), v[i], static_cast<int>(g[i])));
    }
    check_against_reference("motor_rpm", ref.column("motor_rpm"), rpm, 0.0);
}

TEST_CASE("check_upshift / check_downshift nos limites") {
    // Na ultima marcha nao sobe; na primeira nao desce; durante a troca nada muda.
    CHECK(ctl::check_upshift(car(), 100.0, car().gear_count(), false) == car().gear_count());
    CHECK(ctl::check_downshift(car(), 0.0, 1, false) == 1);
    CHECK(ctl::check_upshift(car(), 100.0, 1, true) == 1);
    CHECK(ctl::check_downshift(car(), 0.0, 3, true) == 3);
    CHECK(ctl::check_upshift(car(), 100.0, 1, false) == 2);
    CHECK(ctl::check_downshift(car(), 0.0, 3, false) == 2);
}
