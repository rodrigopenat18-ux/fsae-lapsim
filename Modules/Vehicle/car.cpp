#include "Vehicle/car.hpp"

#include <cmath>
#include <stdexcept>
#include <string>

namespace fsae {

namespace {

constexpr double kPi = 3.141592653589793;  // igual ao `pi` do MATLAB

void require(bool ok, const std::string& message) {
    if (!ok) throw std::invalid_argument("Carro invalido: " + message);
}

void require_positive(double v, const char* name) {
    require(std::isfinite(v) && v > 0.0, std::string(name) + " deve ser > 0");
}

}  // namespace

CarSpec CarSpec::matlab_default() {
    CarSpec s;  // os inicializadores da struct já são os valores do initializeCar.m
    s.name = "Carro 2024";

    // dataEngine.m
    s.engine_rpm = {4499,  4598,  4698,  4792,  4894,  5003,  5098,  5195,  5296,  5400,  5506,
                    5592,  5699,  5806,  5890,  5996,  6101,  6206,  6292,  6401,  6490,  6601,
                    6690,  6799,  6907,  6993,  7099,  7203,  7307,  7390,  7491,  7592,  7694,
                    7797,  7903,  7991,  8103,  8193,  8307,  8396,  8507,  8593,  8701,  8807,
                    8891,  8997,  9102,  9208,  9295,  9404,  9491,  9601,  9667,  9792,  9902,
                    9990,  10098, 10206, 10291, 10397, 10502, 10606, 10709, 10790, 10890, 10990,
                    11108, 11205, 11302, 11397, 11492, 11605, 11697, 11805, 11907, 12006, 12100,
                    12196, 12301, 12397, 12454};
    s.engine_torque_nm = {
        26.38675, 31.81705, 35.1187,  38.1432,  40.9515,  43.42745, 45.01675, 46.2277,  47.4766,
        48.85775, 49.8548,  50.06065, 49.64205, 49.1441,  48.94975, 48.77725, 48.829,   49.3074,
        50.13655, 51.31645, 51.8949,  51.6741,  51.13935, 50.6092,  50.0756,  49.57765, 48.9831,
        48.5507,  48.25515, 47.7572,  47.08675, 46.9982,  47.5962,  48.81635, 50.3608,  51.59015,
        52.82985, 53.11275, 52.6378,  51.98,    50.90015, 50.2757,  49.79845, 49.5144,  49.25565,
        49.12225, 49.4937,  50.14115, 50.6989,  51.18535, 51.44755, 51.50965, 51.3636,  51.01055,
        50.60575, 50.32975, 49.8824,  49.42125, 49.13375, 48.8152,  48.38165, 47.92625, 47.35585,
        46.9453,  46.3358,  46.05405, 45.3583,  44.95005, 44.6614,  44.3785,  43.85525, 43.5298,
        42.4511,  41.5472,  37.0116,  29.88735, 26.9307,  24.51225, 22.7907,  16.307,   14.99715};
    s.bsfc_rpm = {5000, 5500, 6000, 6500, 7500, 8200, 9500, 10500, 11500, 12000, 12454};
    s.bsfc_g_kwh = {450, 380, 385, 365, 400, 340, 355, 395, 430, 450, 460};
    return s;
}

Car Car::from_spec(const CarSpec& s) {
    require_positive(s.car_mass_kg, "car_mass_kg");
    require(std::isfinite(s.driver_mass_kg) && s.driver_mass_kg >= 0.0, "driver_mass_kg deve ser >= 0");
    require_positive(s.gravity_m_s2, "gravity_m_s2");
    require_positive(s.frontal_area_m2, "frontal_area_m2");
    require_positive(s.air_density_kg_m3, "air_density_kg_m3");
    require_positive(s.wheelbase_m, "wheelbase_m");
    require_positive(s.tire_radius_mm, "tire_radius_mm");
    require_positive(s.track_front_m, "track_front_m");
    require_positive(s.track_rear_m, "track_rear_m");
    require_positive(s.primary_reduction, "primary_reduction");
    require_positive(s.final_drive, "final_drive");
    require_positive(s.brake_disc_mass_kg, "brake_disc_mass_kg");
    require_positive(s.brake_disc_diameter_m, "brake_disc_diameter_m");
    require(std::isfinite(s.tyre_load_sensitivity) && s.tyre_load_sensitivity >= 0.0 &&
                s.tyre_load_sensitivity < 1.0,
            "tyre_load_sensitivity deve estar entre 0 e 1");
    require_positive(s.brake_max_torque_nm, "brake_max_torque_nm");
    if (s.brake_bias_front_percent) {
        require(std::isfinite(*s.brake_bias_front_percent) && *s.brake_bias_front_percent > 0.0 &&
                    *s.brake_bias_front_percent < 100.0,
                "brake_bias_front_percent deve estar entre 0 e 100");
    }
    require(!s.gear_ratios.empty(), "gear_ratios vazio");
    for (double r : s.gear_ratios) require_positive(r, "gear_ratios[i]");
    require(s.upshift_rpm.size() == s.gear_ratios.size(),
            "upshift_rpm deve ter um valor por marcha (" + std::to_string(s.gear_ratios.size()) +
                ")");
    require(s.engine_rpm.size() == s.engine_torque_nm.size() && s.engine_rpm.size() >= 2,
            "engine_rpm e engine_torque_nm devem ter o mesmo tamanho (>= 2)");
    require(s.bsfc_rpm.size() == s.bsfc_g_kwh.size() && s.bsfc_rpm.size() >= 2,
            "bsfc_rpm e bsfc_g_kwh devem ter o mesmo tamanho (>= 2)");

    Car c;
    c.name = s.name;

    // Mesmas contas do initializeCar.m, na mesma ordem.
    c.mass_only = s.car_mass_kg;
    c.pilot_mass = s.driver_mass_kg;
    c.mass = c.mass_only + c.pilot_mass;
    c.g = s.gravity_m_s2;

    c.frontal_area = s.frontal_area_m2;
    c.rho = s.air_density_kg_m3;
    c.cl = s.cl_a_m2 / c.frontal_area;
    c.cd = s.cd_a_m2 / c.frontal_area;
    c.cp_aero = s.center_of_pressure_rear_percent;

    c.wheelbase = s.wheelbase_m;
    c.tire_radius = s.tire_radius_mm / 1000.0;
    c.weight_dist_rear_percent = s.weight_dist_rear_percent;
    c.cg_height = s.cg_height_m;
    c.track_front = s.track_front_m;
    c.track_rear = s.track_rear_m;

    c.mux_static = s.mu_x_static;
    c.mux_kinetic = s.mu_x_kinetic;
    c.muy = s.mu_y;
    c.tyre_load_sensitivity = s.tyre_load_sensitivity;

    c.overall_reduction = s.primary_reduction * s.final_drive;
    c.gear_ratios = s.gear_ratios;
    c.powertrain_efficiency = s.powertrain_efficiency;
    c.shift_time = s.shift_time_ms / 1000.0;
    c.shift_rpm = s.upshift_rpm;
    c.launch_rpm = s.launch_rpm;
    c.downshift_rpm = s.downshift_rpm;

    c.engine = EngineMap(s.engine_rpm, s.engine_torque_nm, s.bsfc_rpm, s.bsfc_g_kwh);

    c.brake_mass = s.brake_disc_mass_kg;
    c.brake_diameter = s.brake_disc_diameter_m;
    c.brake_surface_area = s.brake_disc_surface_area_m2;
    c.brake_thermalcond = s.brake_disc_thermal_conductivity_w_mk;
    c.brake_emissivity = s.brake_disc_emissivity;
    // brake_cond_area = num_bobbins * (pi * (diametro_bobbin/2)^2)
    const double r_bobbin = s.brake_bobbin_diameter_m / 2.0;
    c.brake_cond_area = s.brake_bobbin_count * (kPi * (r_bobbin * r_bobbin));
    c.brake_cond_dist = s.brake_conduction_distance_m;
    c.brake_max_torque = s.brake_max_torque_nm;
    if (s.brake_bias_front_percent) c.brake_bias_front = *s.brake_bias_front_percent / 100.0;

    c.air_k = s.air_thermal_conductivity_w_mk;
    c.air_mu = s.air_dynamic_viscosity_pa_s;
    return c;
}

}  // namespace fsae
