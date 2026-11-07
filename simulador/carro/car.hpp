#pragma once

#include <string>
#include <vector>

#include "carro/engine.hpp"

namespace fsae {

/// Dados de entrada do carro, nas unidades em que costumam ser medidos.
/// É o que fica no arquivo JSON; `Car::from_spec` converte para unidades SI
/// fazendo as MESMAS contas do `initializeCar.m` (para reproduzir o MATLAB bit a bit).
struct CarSpec {
    std::string name = "default";

    // --- Massa ---
    double car_mass_kg = 220.0;
    double driver_mass_kg = 75.0;
    double gravity_m_s2 = 9.81;

    // --- Aerodinâmica ---
    double frontal_area_m2 = 1.1;
    double air_density_kg_m3 = 1.18836;
    double cl_a_m2 = 3.03;  ///< Cl·A
    double cd_a_m2 = 1.28;  ///< Cd·A
    double center_of_pressure_rear_percent = 50.0;

    // --- Chassi ---
    double wheelbase_m = 1.550;
    double tire_radius_mm = 228.6;
    double weight_dist_rear_percent = 50.0;
    double cg_height_m = 0.312;
    double track_front_m = 1.2;
    double track_rear_m = 1.15;

    // --- Pneu ---
    double mu_x_static = 2.1;
    double mu_x_kinetic = 1.3;
    double mu_y = 1.2;

    // --- Transmissão ---
    double primary_reduction = 2.111;
    double final_drive = 3.576;
    std::vector<double> gear_ratios = {1.938, 1.556, 1.348, 1.208, 1.095};
    double powertrain_efficiency = 0.95;
    double shift_time_ms = 120.0;
    std::vector<double> upshift_rpm = {12000, 12000, 12000, 12000, 12000};
    double launch_rpm = 8600.0;
    double downshift_rpm = 11500.0;

    // --- Motor ---
    std::vector<double> engine_rpm;
    std::vector<double> engine_torque_nm;
    std::vector<double> bsfc_rpm;
    std::vector<double> bsfc_g_kwh;

    // --- Freio (um disco) ---
    double brake_disc_mass_kg = 0.458;
    double brake_disc_diameter_m = 0.175;
    double brake_disc_surface_area_m2 = 0.03856;
    double brake_disc_thermal_conductivity_w_mk = 16.0;
    double brake_disc_emissivity = 0.55;
    double brake_bobbin_count = 5.0;
    double brake_bobbin_diameter_m = 0.009;
    double brake_conduction_distance_m = 0.0282;

    // --- Ar ---
    double air_thermal_conductivity_w_mk = 0.026;
    double air_dynamic_viscosity_pa_s = 1.8e-5;

    /// Valores do `initializeCar.m` / `dataEngine.m` originais.
    static CarSpec matlab_default();
};

/// Parâmetros do carro em unidades SI, prontos para a simulação.
/// Os nomes dos campos seguem o `initializeCar.m` (entre parênteses).
struct Car {
    std::string name;

    // --- Massa ---
    double mass_only = 0;     ///< massa do carro [kg] (mass_only)
    double pilot_mass = 0;    ///< massa do piloto [kg] (pilot_mass)
    double mass = 0;          ///< massa total [kg] (mass)
    double g = 0;             ///< gravidade [m/s²] (g)

    // --- Aerodinâmica ---
    double frontal_area = 0;  ///< [m²] (frontalArea)
    double rho = 0;           ///< densidade do ar [kg/m³] (rho)
    double cl = 0;            ///< coef. de sustentação (cl = ClA / A)
    double cd = 0;            ///< coef. de arrasto (cd = CdA / A)
    double cp_aero = 0;       ///< centro de pressão, % no eixo traseiro (cpaero)

    // --- Chassi ---
    double wheelbase = 0;                 ///< [m] (wheelbase)
    double tire_radius = 0;               ///< [m] (tire_radius)
    double weight_dist_rear_percent = 0;  ///< [%] (weight_dist_rear_percent)
    double cg_height = 0;                 ///< [m] (cg_height)
    double track_front = 0;               ///< bitola dianteira [m] (track_front_m)
    double track_rear = 0;                ///< bitola traseira [m] (track_rear_m)

    // --- Pneu ---
    double mux_static = 0;   ///< atrito longitudinal estático (mux_static)
    double mux_kinetic = 0;  ///< atrito longitudinal cinético (mux_kinetic)
    double muy = 0;          ///< atrito lateral (muy)

    // --- Transmissão ---
    double overall_reduction = 0;      ///< primária × final (overall_reduction)
    std::vector<double> gear_ratios;   ///< (gear_ratios)
    double powertrain_efficiency = 0;  ///< (powertrain_efficiency)
    double shift_time = 0;             ///< [s] (shiftTime)
    std::vector<double> shift_rpm;     ///< rpm de subida por marcha (shiftRPM_vec)
    double launch_rpm = 0;             ///< (launchRPM)
    double downshift_rpm = 0;          ///< (downshiftRPM)

    // --- Motor ---
    EngineMap engine;  ///< (TorqueInterpol, bsfc_map, rpm_min, rpm_max)

    // --- Freio ---
    double brake_mass = 0;          ///< massa de um disco [kg] (brake_mass)
    double brake_diameter = 0;      ///< [m] (brake_diameter)
    double brake_surface_area = 0;  ///< [m²] (brake_surface_area)
    double brake_thermalcond = 0;   ///< condutividade do disco [W/m·K] (brake_thermalcond)
    double brake_emissivity = 0;    ///< (brake_emissivity)
    double brake_cond_area = 0;     ///< área de condução disco→cubo [m²] (brake_cond_area)
    double brake_cond_dist = 0;     ///< [m] (brake_cond_dist)

    // --- Ar ---
    double air_k = 0;   ///< condutividade térmica do ar [W/m·K] (air_k)
    double air_mu = 0;  ///< viscosidade dinâmica do ar [kg/m·s] (air_mu)

    [[nodiscard]] int gear_count() const noexcept { return static_cast<int>(gear_ratios.size()); }

    /// Converte e valida. Lança std::invalid_argument se os dados forem inconsistentes.
    static Car from_spec(const CarSpec& spec);

    /// Carro do `initializeCar.m` original.
    static Car matlab_default() { return from_spec(CarSpec::matlab_default()); }
};

}  // namespace fsae
