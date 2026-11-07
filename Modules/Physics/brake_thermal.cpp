#include "Physics/brake_thermal.hpp"

#include <cmath>

#include "Math/interp.hpp"

namespace fsae::physics {

namespace {

/// Calor específico do disco [J/kg·K] em função da temperatura [°C].
const math::LinearInterpolant& disc_specific_heat() {
    static const math::LinearInterpolant cp(
        {25, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000},
        {490, 515, 550, 580, 605, 630, 655, 680, 705, 725, 745});
    return cp;
}

}  // namespace

BrakeTempResult brake_temperature(const Car& car, double velocity, double brake_force_total,
                                  double current_temp, double dt, double front_load_ratio) {
    BrakeTempResult r;

    // 1. Calor gerado (por disco: metade da potência do eixo dianteiro)
    const double power_total = brake_force_total * velocity;
    const double power_front = power_total * front_load_ratio;
    r.p_in = power_front / 2;

    // 2. Geometria e propriedades
    const double r_tire = car.tire_radius;
    const double rho = car.rho;
    const double mu = car.air_mu;
    const double D = car.brake_diameter;
    const double k_air = car.air_k;
    const double k_disc = car.brake_thermalcond;
    const double A_sup = car.brake_surface_area;
    const double m_disc = car.brake_mass;
    const double epsilon = car.brake_emissivity;
    const double A_cond = car.brake_cond_area;
    const double L_path = car.brake_cond_dist;

    // 3. Convecção translacional + rotacional (a potência acima usa a velocidade original)
    if (velocity < 0.1) velocity = 0.1;

    const double re_trans = (rho * velocity * D) / mu;
    const double nu_trans =
        re_trans <= 2.4e5 ? 0.70 * std::pow(re_trans, 0.55) : 0.037 * std::pow(re_trans, 0.8);

    const double omega = velocity / r_tire;
    const double half_d = D / 2;
    const double re_rot = (rho * omega * (half_d * half_d)) / mu;
    const double nu_rot = 0.40 * std::pow(re_rot, 0.5);

    const double nu_total = std::sqrt(nu_trans * nu_trans + nu_rot * nu_rot);
    constexpr double kTurbulenceFactor = 3.0;
    r.h = (nu_total * k_air / D) * kTurbulenceFactor;

    // 4. Dissipação
    constexpr double kTAmb = 25;
    constexpr double kTHub = 60;
    const double t_k = current_temp + 273.15;
    const double tamb_k = kTAmb + 273.15;
    const double thub_k = kTHub + 273.15;

    r.p_conv = r.h * A_sup * (current_temp - kTAmb);

    constexpr double kSigma = 5.67e-8;
    r.p_rad = epsilon * kSigma * A_sup * (std::pow(t_k, 4) - std::pow(tamb_k, 4));

    r.p_cond = current_temp > kTHub ? (k_disc * A_cond) * (t_k - thub_k) / L_path : 0.0;

    const double p_out_total = r.p_conv + r.p_rad + r.p_cond;

    // 5. Integração explícita com calor específico variável
    const double cp = disc_specific_heat()(current_temp);
    const double rate_of_change = (r.p_in - p_out_total) / (m_disc * cp);
    r.new_temp = current_temp + (rate_of_change * dt);
    return r;
}

}  // namespace fsae::physics
