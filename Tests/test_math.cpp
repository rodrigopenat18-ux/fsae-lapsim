#include <doctest/doctest.h>

#include <vector>

#include "Math/interp.hpp"
#include "support/reference.hpp"

using fsae::math::LinearInterpolant;
using fsae::math::linspace;
using fsae::math::PchipInterpolant;
using fsae::test::check_against_reference;
using fsae::test::load_reference;

namespace {

// Estas funções só usam + − × ÷ (sem sin/cos/pow), então exigimos igualdade
// bit a bit com o MATLAB.
constexpr double kRtol = 0.0;

template <typename F>
std::vector<double> eval(const F& f, const std::vector<double>& xq) {
    std::vector<double> out;
    out.reserve(xq.size());
    for (double x : xq) out.push_back(f(x));
    return out;
}

}  // namespace

TEST_CASE("pchip: curva de torque do motor (griddedInterpolant 'pchip', com extrapolacao)") {
    const auto data = load_reference("unit/engine_torque_data.csv");
    const PchipInterpolant torque(data.column("rpm"), data.column("torque_nm"));

    const auto ref = load_reference("unit/engine_eval.csv");
    check_against_reference("torque pchip", ref.column("torque_nm"),
                            eval(torque, ref.column("rpm")), kRtol);
}

TEST_CASE("linear: BSFC do motor (griddedInterpolant 'linear', com extrapolacao)") {
    const auto data = load_reference("unit/engine_bsfc_data.csv");
    const LinearInterpolant bsfc(data.column("rpm"), data.column("bsfc_g_kwh"));

    const auto ref = load_reference("unit/engine_eval.csv");
    check_against_reference("bsfc linear", ref.column("bsfc_g_kwh"),
                            eval(bsfc, ref.column("rpm")), kRtol);
}

TEST_CASE("linear: tabela de Cp do disco (interp1 'linear', 'extrap')") {
    const LinearInterpolant cp({25, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000},
                               {490, 515, 550, 580, 605, 630, 655, 680, 705, 725, 745});
    const auto ref = load_reference("unit/interp1_linear_extrap.csv");
    check_against_reference("cp interp1", ref.column("y"), eval(cp, ref.column("x")), kRtol);
}

TEST_CASE("pchip e linear: conjuntos pequenos (casos de borda)") {
    const std::vector<std::vector<double>> xs = {
        {0, 1}, {0, 1, 3}, {0, 1, 2, 4, 5}, {-2, 0.5, 1, 7}};
    const std::vector<std::vector<double>> ys = {
        {2, 5}, {0, 2, 1}, {1, 1, 3, 3, 0}, {4, -1, -1.5, 10}};

    const auto ref = load_reference("unit/gridded_small.csv");
    const auto& set = ref.column("set");
    const auto& x = ref.column("x");

    for (std::size_t s = 0; s < xs.size(); ++s) {
        const PchipInterpolant p(xs[s], ys[s]);
        const LinearInterpolant l(xs[s], ys[s]);
        std::vector<double> xq, exp_p, exp_l;
        for (std::size_t i = 0; i < ref.rows(); ++i) {
            if (static_cast<std::size_t>(set[i]) == s + 1) {
                xq.push_back(x[i]);
                exp_p.push_back(ref.column("pchip")[i]);
                exp_l.push_back(ref.column("linear")[i]);
            }
        }
        const std::string tag = "conjunto " + std::to_string(s + 1);
        check_against_reference(tag + " pchip", exp_p, eval(p, xq), kRtol);
        check_against_reference(tag + " linear", exp_l, eval(l, xq), kRtol);
    }
}

TEST_CASE("linspace") {
    const auto ref = load_reference("unit/linspace.csv");
    const auto& cs = ref.column("case");
    std::vector<double> expected, actual;
    std::size_t i = 0;
    while (i < ref.rows()) {
        const double a = ref.column("a")[i];
        const double b = ref.column("b")[i];
        const auto n = static_cast<std::size_t>(ref.column("n")[i]);
        const auto y = linspace(a, b, n);
        REQUIRE(y.size() == n);
        for (std::size_t k = 0; k < n; ++k, ++i) {
            REQUIRE(cs[i] == cs[i - k]);
            expected.push_back(ref.column("value")[i]);
            actual.push_back(y[k]);
        }
    }
    check_against_reference("linspace", expected, actual, kRtol);
}

TEST_CASE("validacao de entrada dos interpoladores") {
    CHECK_THROWS(PchipInterpolant({0.0}, {1.0}));
    CHECK_THROWS(PchipInterpolant({0.0, 0.0}, {1.0, 2.0}));
    CHECK_THROWS(LinearInterpolant({0.0, 1.0}, {1.0}));
    CHECK(linspace(0.0, 1.0, 0).empty());
    CHECK(linspace(0.0, 1.0, 1) == std::vector<double>{1.0});
}
