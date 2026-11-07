#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "arquivos/csv.hpp"
#include "fisica/brake_thermal.hpp"
#include "fisica/load_transfer.hpp"
#include "fisica/powertrain.hpp"
#include "fisica/resistances.hpp"
#include "carro/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
using fsae::test::reference_path;
namespace ph = fsae::physics;

namespace {

/// Tolerância das funções que só usam + − × ÷ (e sqrt, que é exata no IEEE 754):
/// igualdade bit a bit com o MATLAB.
constexpr double kExact = 0.0;

/// Funções com sin/cos/pow dependem da biblioteca matemática (libm) de cada
/// plataforma. No Windows + MSYS2/GCC o resultado é idêntico ao MATLAB R2026a,
/// mas outras libm (glibc, macOS, MSVC) podem divergir no último bit (~1e-16
/// relativo); a tolerância evita falso alarme no CI dessas plataformas.
constexpr double kLibm = 1e-15;

const Car& car() {
    static const Car c = Car::matlab_default();
    return c;
}

}  // namespace

TEST_CASE("arrasto, downforce, rampa e rolamento (primitives.csv)") {
    const auto ref = load_reference("unit/primitives.csv");
    const auto& v = ref.column("v");
    const auto& s = ref.column("slope");
    std::vector<double> drag, down, slope_f, roll;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        drag.push_back(ph::drag(car(), v[i]));
        down.push_back(ph::downforce(car(), v[i]));
        slope_f.push_back(ph::slope_force(car(), s[i]));
        roll.push_back(ph::tyre_resistance(car(), v[i], s[i], down.back()));
    }
    check_against_reference("drag", ref.column("drag"), drag, kExact);
    check_against_reference("downforce", ref.column("downforce"), down, kExact);
    check_against_reference("slope_force", ref.column("slope_force"), slope_f, kLibm);
    check_against_reference("rolling_resistance", ref.column("rolling_resistance"), roll, kLibm);
}

TEST_CASE("cargas nos pneus (load_transfer.csv)") {
    const auto ref = load_reference("unit/load_transfer.csv");
    const auto& v = ref.column("v");
    const auto& s = ref.column("slope");
    const auto& ax = ref.column("ax_g");
    const auto& ay = ref.column("ay_g");

    std::vector<std::vector<double>> st(4), dyn(4), normal(4), dist(4);
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        const auto a = ph::static_loads(car(), s[i]);
        const auto b = ph::dynamic_transfer(car(), ax[i], ay[i]);
        const auto c = ph::load_transfer(car(), ax[i], ay[i], s[i], v[i]);
        for (std::size_t w = 0; w < 4; ++w) {
            st[w].push_back(a[w]);
            dyn[w].push_back(b[w]);
            normal[w].push_back(c.normal_n[w]);
            dist[w].push_back(c.distribution[w]);
        }
    }
    for (std::size_t w = 0; w < 4; ++w) {
        const std::string n = std::to_string(w + 1);
        check_against_reference("static" + n, ref.column("static" + n), st[w], kLibm);
        check_against_reference("dyn" + n, ref.column("dyn" + n), dyn[w], kExact);
        check_against_reference("normal" + n, ref.column("normal" + n), normal[w], kLibm);
        check_against_reference("dist" + n, ref.column("dist" + n), dist[w], kLibm);
    }
}

TEST_CASE("rpm, torque na roda e forca do motor (powertrain.csv)") {
    const auto ref = load_reference("unit/powertrain.csv");
    const auto& v = ref.column("v");
    const auto& gear = ref.column("gear");
    std::vector<double> rpm, torque, force;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        const int g = static_cast<int>(gear[i]);
        rpm.push_back(ph::engine_rpm(car(), v[i], g));
        torque.push_back(ph::wheel_torque(car(), rpm.back(), g));
        force.push_back(ph::engine_force(car(), torque.back()));
    }
    check_against_reference("rpm", ref.column("rpm"), rpm, kExact);
    check_against_reference("wheel_torque", ref.column("wheel_torque"), torque, kExact);
    check_against_reference("engine_force", ref.column("engine_force"), force, kExact);
}

TEST_CASE("torque de largada e forca de freio (scalars.csv)") {
    const auto ref = fsae::io::read_key_value_csv(reference_path("unit/scalars.csv"));
    CHECK(ph::launch_torque(car()) == ref.at("launch_torque_nm"));
    CHECK(ph::brake_force(car()) == ref.at("brake_force_n"));
}

TEST_CASE("aderencia longitudinal maxima (max_grip.csv)") {
    const auto ref = load_reference("unit/max_grip.csv");
    const auto& f = ref.column("engine_force");
    const auto& n = ref.column("normal_rear");
    std::vector<double> grip;
    for (std::size_t i = 0; i < ref.rows(); ++i) grip.push_back(ph::max_grip(car(), f[i], n[i]));
    check_against_reference("max_grip", ref.column("max_grip"), grip, kExact);
}

TEST_CASE("temperatura do disco de freio (brake_temp.csv)") {
    const auto ref = load_reference("unit/brake_temp.csv");
    const auto& v = ref.column("v");
    const auto& fb = ref.column("brake_force");
    const auto& t = ref.column("temp");
    const auto& dt = ref.column("dt");
    const auto& q = ref.column("front_ratio");
    std::vector<double> t1, pin, pconv, prad, pcond, h;
    for (std::size_t i = 0; i < ref.rows(); ++i) {
        const auto r = ph::brake_temperature(car(), v[i], fb[i], t[i], dt[i], q[i]);
        t1.push_back(r.new_temp);
        pin.push_back(r.p_in);
        pconv.push_back(r.p_conv);
        prad.push_back(r.p_rad);
        pcond.push_back(r.p_cond);
        h.push_back(r.h);
    }
    check_against_reference("p_in", ref.column("p_in"), pin, kExact);
    check_against_reference("p_cond", ref.column("p_cond"), pcond, kExact);
    check_against_reference("h", ref.column("h"), h, kLibm);
    check_against_reference("p_conv", ref.column("p_conv"), pconv, kLibm);
    check_against_reference("p_rad", ref.column("p_rad"), prad, kLibm);
    check_against_reference("new_temp", ref.column("new_temp"), t1, kLibm);
}
