#include <doctest/doctest.h>

#include <cstdio>
#include <string>
#include <vector>

#include "Simulation/straight.hpp"
#include "Vehicle/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
namespace sim = fsae::sim;

namespace {

/// sin/cos/pow podem divergir no último bit em outras libm (ver test_physics.cpp).
/// Na reta o erro se propaga por centenas de passos, então a folga é maior.
constexpr double kLibm = 1e-12;

const Car& car() {
    static const Car c = Car::matlab_default();
    return c;
}

std::vector<double> as_double(const std::vector<int>& v) { return {v.begin(), v.end()}; }

}  // namespace

TEST_CASE("reta completa: 10 casos do MATLAB (straight/)") {
    const auto cases = load_reference("straight/cases.csv");
    const auto totals = load_reference("straight/totals.csv");

    for (std::size_t k = 0; k < cases.rows(); ++k) {
        char name[32];
        std::snprintf(name, sizeof name, "straight/case_%02zu.csv", k + 1);
        CAPTURE(name);

        const double length = cases.column("length")[k];
        const double v0 = cases.column("v0")[k];
        const double target = cases.column("target")[k];
        const double slope = cases.column("slope")[k];
        const double temp0 = cases.column("temp0")[k];

        const auto r = sim::simulate_straight(car(), length, v0, target, slope, temp0);
        const auto ref = load_reference(name);
        const auto& h = r.history;

        CHECK(r.total_time == totals.column("total_time")[k]);
        REQUIRE(h.size() == ref.rows());

        const std::string p = std::string(name) + " ";
        check_against_reference(p + "time", ref.column("time"), h.time, 0.0);
        check_against_reference(p + "position", ref.column("position"), h.position, kLibm);
        check_against_reference(p + "velocity", ref.column("velocity"), h.velocity, kLibm);
        check_against_reference(p + "acceleration", ref.column("acceleration"), h.acceleration,
                                kLibm);
        check_against_reference(p + "gear", ref.column("gear"), as_double(h.gear), 0.0);
        check_against_reference(p + "rpm", ref.column("rpm"), h.rpm, kLibm);
        check_against_reference(p + "drag", ref.column("drag"), h.drag, kLibm);
        check_against_reference(p + "downforce", ref.column("downforce"), h.downforce, kLibm);
        check_against_reference(p + "rolling_resistance", ref.column("rolling_resistance"),
                                h.rolling_resistance, kLibm);
        check_against_reference(p + "slope_force", ref.column("slope_force"), h.slope_force,
                                kLibm);
        check_against_reference(p + "engine_force", ref.column("engine_force"), h.engine_force,
                                kLibm);
        check_against_reference(p + "brake_force", ref.column("brake_force"), h.brake_force,
                                kLibm);
        check_against_reference(p + "brake_temp", ref.column("brake_temp"), h.brake_temp, kLibm);
        check_against_reference(p + "h_coef", ref.column("h_coef"), h.h_coef, kLibm);
        check_against_reference(p + "fuel_consumed_g", ref.column("fuel_consumed_g"),
                                h.fuel_consumed_g, kLibm);
        check_against_reference(p + "P_conv", ref.column("P_conv"), h.p_conv, kLibm);
        check_against_reference(p + "P_rad", ref.column("P_rad"), h.p_rad, kLibm);
        check_against_reference(p + "P_cond", ref.column("P_cond"), h.p_cond, kLibm);
        check_against_reference(p + "load_front_dist", ref.column("load_front_dist"),
                                h.load_front_dist, kLibm);
    }
}
