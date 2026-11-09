#include "Tracks/corner_speed.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Physics/load_transfer.hpp"
#include "Physics/powertrain.hpp"

namespace fsae::track {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

/// Último índice k tal que a + k·d ≤ b no operador `a:d:b` do MATLAB, que tolera
/// erro de arredondamento no extremo final (2·eps·max(|a|,|b|)). -1 se vazio.
long long colon_last_index(double a, double d, double b) {
    if (!(b >= a)) return -1;
    const double tol = 2.0 * std::numeric_limits<double>::epsilon() * std::max(std::abs(a), std::abs(b));
    auto n = static_cast<long long>(std::round((b - a) / d));
    if (a + static_cast<double>(n) * d - b > tol) --n;
    return n;
}

}  // namespace

CornerSpeed max_safe_corner_speed(const Car& car, double radius, double slope) noexcept {
    CornerSpeed r;

    // 1. Aderência
    const double k_aero = 0.5 * car.rho * car.cl * car.frontal_area;
    const double fn_weight = car.mass * car.g * std::cos(slope);
    const double denominator = car.mass - car.muy * k_aero * radius;
    if (car.model.unbounded_corner_grip && denominator <= 0) {
        // m·v²/R ≤ μy·(m·g·cosθ + K·v²) vale para qualquer v: o downforce cresce mais rápido
        // que a força centrípeta necessária, então a aderência nunca limita esta curva.
        r.v_grip = kInf;
    } else {
        const double v_sq_grip = (car.muy * fn_weight * radius) / denominator;
        r.v_grip = std::sqrt(std::max(0.0, v_sq_grip));
    }

    // 2. Tombamento (roda descarregada)
    r.v_tip = r.v_grip;
    constexpr double kStart = 1;
    constexpr double kStep = 0.5;
    // Com a correção 2, o teste vai só até a velocidade que o carro consegue atingir.
    const double v_test_max =
        car.model.unbounded_corner_grip ? std::min(r.v_grip, physics::max_vehicle_speed(car)) : r.v_grip;
    if (std::isfinite(v_test_max)) {
        const long long last = colon_last_index(kStart, kStep, v_test_max);
        for (long long k = 0; k <= last; ++k) {
            const double v_test = kStart + static_cast<double>(k) * kStep;
            const double acc_lat_g = (v_test * v_test / radius) / car.g;
            const auto lt = physics::load_transfer(car, 0, acc_lat_g, slope, v_test);
            const auto& n = lt.normal_n;
            if (n[0] <= 0.01 || n[1] <= 0.01 || n[2] <= 0.01 || n[3] <= 0.01) {
                r.v_tip = v_test - kStep;
                break;
            }
        }
    }

    // 3. Resultado
    r.v_max = std::min(r.v_grip, r.v_tip);
    return r;
}

std::vector<CornerSpeed> corner_speeds(const Car& car, const std::vector<Segment>& segments) {
    std::vector<CornerSpeed> out;
    out.reserve(segments.size());
    for (const auto& s : segments) {
        if (s.radius == 0) {
            out.push_back({kInf, kInf, kInf});
            continue;
        }
        // Original: atan(slope/100), como se o valor fosse porcentagem (problema conhecido 1).
        const double theta = car.model.slope_in_radians ? s.slope : std::atan(s.slope / 100);
        out.push_back(max_safe_corner_speed(car, s.radius, theta));
    }
    return out;
}

double next_corner_speed(const std::vector<Segment>& segments,
                         const std::vector<double>& corner_velocity, std::size_t index) noexcept {
    for (std::size_t j = index + 1; j < segments.size(); ++j) {
        if (segments[j].radius > 0) return corner_velocity[j];
    }
    return kInf;
}

}  // namespace fsae::track
