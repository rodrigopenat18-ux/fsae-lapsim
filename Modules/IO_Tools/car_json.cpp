#include "IO_Tools/car_json.hpp"

#include <fstream>
#include <initializer_list>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace fsae::io {

namespace {

using nlohmann::ordered_json;

class Reader {
public:
    Reader(const ordered_json& obj, std::string where) : obj_(obj), where_(std::move(where)) {
        if (!obj_.is_object()) fail(where_, "esperado um objeto JSON");
    }

    Reader child(const std::string& key) {
        return Reader(field(key), where_ + "." + key);
    }

    double number(const std::string& key) {
        const auto& v = field(key);
        if (!v.is_number()) fail(where_ + "." + key, "esperado um numero");
        return v.get<double>();
    }

    /// Campo opcional: o valor, ou `fallback` se ausente.
    double number_or(const std::string& key, double fallback) {
        return obj_.find(key) == obj_.end() ? fallback : number(key);
    }

    /// Campo opcional que aceita `null`: vazio se ausente ou nulo.
    std::optional<double> optional_number(const std::string& key) {
        const auto it = obj_.find(key);
        if (it == obj_.end() || it->is_null()) return std::nullopt;
        return number(key);
    }

    std::string text(const std::string& key) {
        const auto& v = field(key);
        if (!v.is_string()) fail(where_ + "." + key, "esperado um texto");
        return v.get<std::string>();
    }

    std::vector<double> numbers(const std::string& key) {
        const auto& v = field(key);
        if (!v.is_array()) fail(where_ + "." + key, "esperado uma lista de numeros");
        std::vector<double> out;
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (!v[i].is_number()) {
                fail(where_ + "." + key + "[" + std::to_string(i) + "]", "esperado um numero");
            }
            out.push_back(v[i].get<double>());
        }
        return out;
    }

    /// Declara os campos do grupo: `keys` obrigatórios e `optional` opcionais. Primeiro acusa
    /// campos desconhecidos (provável erro de digitação, com a lista dos válidos), depois
    /// campos obrigatórios ausentes.
    void expect(std::initializer_list<const char*> keys,
                std::initializer_list<const char*> optional = {}) const {
        std::set<std::string> valid(keys.begin(), keys.end());
        valid.insert(optional.begin(), optional.end());
        for (const auto& item : obj_.items()) {
            if (valid.count(item.key()) == 0) {
                std::string list;
                for (const auto& k : keys) list += (list.empty() ? "" : ", ") + std::string(k);
                for (const auto& k : optional) list += ", " + std::string(k);
                fail(where_ + "." + item.key(), "campo desconhecido (validos: " + list + ")");
            }
        }
        for (const auto& k : keys) {
            if (obj_.find(k) == obj_.end()) fail(where_ + "." + k, "campo obrigatorio ausente");
        }
    }

private:
    const ordered_json& field(const std::string& key) {
        const auto it = obj_.find(key);
        if (it == obj_.end()) fail(where_ + "." + key, "campo obrigatorio ausente");
        return *it;
    }

    [[noreturn]] static void fail(const std::string& where, const std::string& msg) {
        throw std::runtime_error("JSON do carro: " + where + ": " + msg);
    }

    const ordered_json& obj_;
    std::string where_;
};

CarSpec from_json(const ordered_json& j, const std::string& source) {
    CarSpec s;
    Reader root(j, source);
    root.expect({"name", "mass", "gravity_m_s2", "aero", "chassis", "tyre", "powertrain", "engine",
                 "brakes", "air"});
    s.name = root.text("name");
    s.gravity_m_s2 = root.number("gravity_m_s2");
    {
        auto r = root.child("mass");
        r.expect({"car_kg", "driver_kg"});
        s.car_mass_kg = r.number("car_kg");
        s.driver_mass_kg = r.number("driver_kg");
    }
    {
        auto r = root.child("aero");
        r.expect({"frontal_area_m2", "air_density_kg_m3", "cl_a_m2", "cd_a_m2",
                  "center_of_pressure_rear_percent"});
        s.frontal_area_m2 = r.number("frontal_area_m2");
        s.air_density_kg_m3 = r.number("air_density_kg_m3");
        s.cl_a_m2 = r.number("cl_a_m2");
        s.cd_a_m2 = r.number("cd_a_m2");
        s.center_of_pressure_rear_percent = r.number("center_of_pressure_rear_percent");
    }
    {
        auto r = root.child("chassis");
        r.expect({"wheelbase_m", "tire_radius_mm", "weight_dist_rear_percent", "cg_height_m",
                  "track_front_m", "track_rear_m"});
        s.wheelbase_m = r.number("wheelbase_m");
        s.tire_radius_mm = r.number("tire_radius_mm");
        s.weight_dist_rear_percent = r.number("weight_dist_rear_percent");
        s.cg_height_m = r.number("cg_height_m");
        s.track_front_m = r.number("track_front_m");
        s.track_rear_m = r.number("track_rear_m");
    }
    {
        auto r = root.child("tyre");
        r.expect({"mu_x_static", "mu_x_kinetic", "mu_y"}, {"load_sensitivity"});
        s.mu_x_static = r.number("mu_x_static");
        s.mu_x_kinetic = r.number("mu_x_kinetic");
        s.mu_y = r.number("mu_y");
        s.tyre_load_sensitivity = r.number_or("load_sensitivity", 0.0);
    }
    {
        auto r = root.child("powertrain");
        r.expect({"primary_reduction", "final_drive", "gear_ratios", "efficiency", "shift_time_ms",
                  "upshift_rpm", "launch_rpm", "downshift_rpm"});
        s.primary_reduction = r.number("primary_reduction");
        s.final_drive = r.number("final_drive");
        s.gear_ratios = r.numbers("gear_ratios");
        s.powertrain_efficiency = r.number("efficiency");
        s.shift_time_ms = r.number("shift_time_ms");
        s.upshift_rpm = r.numbers("upshift_rpm");
        s.launch_rpm = r.number("launch_rpm");
        s.downshift_rpm = r.number("downshift_rpm");
    }
    {
        auto r = root.child("engine");
        r.expect({"rpm", "torque_nm", "bsfc_rpm", "bsfc_g_kwh"});
        s.engine_rpm = r.numbers("rpm");
        s.engine_torque_nm = r.numbers("torque_nm");
        s.bsfc_rpm = r.numbers("bsfc_rpm");
        s.bsfc_g_kwh = r.numbers("bsfc_g_kwh");
    }
    {
        auto r = root.child("brakes");
        r.expect({"disc_mass_kg", "disc_diameter_m", "disc_surface_area_m2",
                  "disc_thermal_conductivity_w_mk", "disc_emissivity", "bobbin_count",
                  "bobbin_diameter_m", "conduction_distance_m"},
                 {"max_torque_nm", "bias_front_percent"});
        s.brake_disc_mass_kg = r.number("disc_mass_kg");
        s.brake_disc_diameter_m = r.number("disc_diameter_m");
        s.brake_disc_surface_area_m2 = r.number("disc_surface_area_m2");
        s.brake_disc_thermal_conductivity_w_mk = r.number("disc_thermal_conductivity_w_mk");
        s.brake_disc_emissivity = r.number("disc_emissivity");
        s.brake_bobbin_count = r.number("bobbin_count");
        s.brake_bobbin_diameter_m = r.number("bobbin_diameter_m");
        s.brake_conduction_distance_m = r.number("conduction_distance_m");
        s.brake_max_torque_nm = r.number_or("max_torque_nm", 3000.0);
        s.brake_bias_front_percent = r.optional_number("bias_front_percent");
    }
    {
        auto r = root.child("air");
        r.expect({"thermal_conductivity_w_mk", "dynamic_viscosity_pa_s"});
        s.air_thermal_conductivity_w_mk = r.number("thermal_conductivity_w_mk");
        s.air_dynamic_viscosity_pa_s = r.number("dynamic_viscosity_pa_s");
    }
    return s;
}

ordered_json to_json(const CarSpec& s) {
    ordered_json j;
    j["name"] = s.name;
    j["mass"] = {{"car_kg", s.car_mass_kg}, {"driver_kg", s.driver_mass_kg}};
    j["gravity_m_s2"] = s.gravity_m_s2;
    j["aero"] = {{"frontal_area_m2", s.frontal_area_m2},
                 {"air_density_kg_m3", s.air_density_kg_m3},
                 {"cl_a_m2", s.cl_a_m2},
                 {"cd_a_m2", s.cd_a_m2},
                 {"center_of_pressure_rear_percent", s.center_of_pressure_rear_percent}};
    j["chassis"] = {{"wheelbase_m", s.wheelbase_m},
                    {"tire_radius_mm", s.tire_radius_mm},
                    {"weight_dist_rear_percent", s.weight_dist_rear_percent},
                    {"cg_height_m", s.cg_height_m},
                    {"track_front_m", s.track_front_m},
                    {"track_rear_m", s.track_rear_m}};
    j["tyre"] = {{"mu_x_static", s.mu_x_static},
                 {"mu_x_kinetic", s.mu_x_kinetic},
                 {"mu_y", s.mu_y},
                 {"load_sensitivity", s.tyre_load_sensitivity}};
    j["powertrain"] = {{"primary_reduction", s.primary_reduction},
                       {"final_drive", s.final_drive},
                       {"gear_ratios", s.gear_ratios},
                       {"efficiency", s.powertrain_efficiency},
                       {"shift_time_ms", s.shift_time_ms},
                       {"upshift_rpm", s.upshift_rpm},
                       {"launch_rpm", s.launch_rpm},
                       {"downshift_rpm", s.downshift_rpm}};
    j["engine"] = {{"rpm", s.engine_rpm},
                   {"torque_nm", s.engine_torque_nm},
                   {"bsfc_rpm", s.bsfc_rpm},
                   {"bsfc_g_kwh", s.bsfc_g_kwh}};
    j["brakes"] = {{"disc_mass_kg", s.brake_disc_mass_kg},
                   {"disc_diameter_m", s.brake_disc_diameter_m},
                   {"disc_surface_area_m2", s.brake_disc_surface_area_m2},
                   {"disc_thermal_conductivity_w_mk", s.brake_disc_thermal_conductivity_w_mk},
                   {"disc_emissivity", s.brake_disc_emissivity},
                   {"bobbin_count", s.brake_bobbin_count},
                   {"bobbin_diameter_m", s.brake_bobbin_diameter_m},
                   {"conduction_distance_m", s.brake_conduction_distance_m},
                   {"max_torque_nm", s.brake_max_torque_nm},
                   {"bias_front_percent", s.brake_bias_front_percent ? ordered_json(*s.brake_bias_front_percent) : ordered_json(nullptr)}};
    j["air"] = {{"thermal_conductivity_w_mk", s.air_thermal_conductivity_w_mk},
                {"dynamic_viscosity_pa_s", s.air_dynamic_viscosity_pa_s}};
    return j;
}

}  // namespace

CarSpec parse_car_spec(const std::string& json_text, const std::string& source) {
    ordered_json j;
    try {
        j = ordered_json::parse(json_text, nullptr, true, /*ignore_comments=*/true);
    } catch (const nlohmann::json::parse_error& e) {
        throw std::runtime_error("JSON do carro: " + source + ": erro de sintaxe: " + e.what());
    }
    return from_json(j, source);
}

CarSpec load_car_spec(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Nao foi possivel abrir " + path.string());
    std::ostringstream ss;
    ss << in.rdbuf();
    return parse_car_spec(ss.str(), path.filename().string());
}

Car load_car(const std::filesystem::path& path) { return Car::from_spec(load_car_spec(path)); }

std::string car_spec_to_json(const CarSpec& spec) { return to_json(spec).dump(2) + "\n"; }

void save_car_spec(const CarSpec& spec, const std::filesystem::path& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Nao foi possivel gravar " + path.string());
    out << car_spec_to_json(spec);
}

}  // namespace fsae::io
