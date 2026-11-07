#include <doctest/doctest.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "fsae/io/car_json.hpp"
#include "fsae/io/csv.hpp"
#include "fsae/vehicle/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::CarSpec;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
using fsae::test::reference_path;

namespace {

/// Pares (chave do car_params.csv do MATLAB, valor no C++).
std::vector<std::pair<std::string, double>> matlab_view(const Car& c) {
    std::vector<std::pair<std::string, double>> v = {
        {"mass_only", c.mass_only},
        {"pilot_mass", c.pilot_mass},
        {"mass", c.mass},
        {"g", c.g},
        {"frontalArea", c.frontal_area},
        {"rho", c.rho},
        {"cl", c.cl},
        {"cd", c.cd},
        {"cpaero", c.cp_aero},
        {"wheelbase", c.wheelbase},
        {"tire_radius", c.tire_radius},
        {"weight_dist_rear_percent", c.weight_dist_rear_percent},
        {"cg_height", c.cg_height},
        {"track_front_m", c.track_front},
        {"track_rear_m", c.track_rear},
        {"mux_static", c.mux_static},
        {"mux_kinetic", c.mux_kinetic},
        {"muy", c.muy},
        {"overall_reduction", c.overall_reduction},
        {"powertrain_efficiency", c.powertrain_efficiency},
        {"shiftTime", c.shift_time},
        {"launchRPM", c.launch_rpm},
        {"downshiftRPM", c.downshift_rpm},
        {"rpm_min", c.engine.rpm_min()},
        {"rpm_max", c.engine.rpm_max()},
        {"brake_mass", c.brake_mass},
        {"brake_diameter", c.brake_diameter},
        {"brake_surface_area", c.brake_surface_area},
        {"brake_thermalcond", c.brake_thermalcond},
        {"brake_emissivity", c.brake_emissivity},
        {"brake_cond_area", c.brake_cond_area},
        {"brake_cond_dist", c.brake_cond_dist},
        {"air_k", c.air_k},
        {"air_mu", c.air_mu},
    };
    for (std::size_t i = 0; i < c.gear_ratios.size(); ++i) {
        v.emplace_back("gear_ratios(" + std::to_string(i + 1) + ")", c.gear_ratios[i]);
    }
    for (std::size_t i = 0; i < c.shift_rpm.size(); ++i) {
        v.emplace_back("shiftRPM_vec(" + std::to_string(i + 1) + ")", c.shift_rpm[i]);
    }
    return v;
}

void check_same_car(const Car& a, const Car& b) {
    const auto va = matlab_view(a);
    const auto vb = matlab_view(b);
    REQUIRE(va.size() == vb.size());
    for (std::size_t i = 0; i < va.size(); ++i) {
        INFO(va[i].first);
        CHECK(va[i].second == vb[i].second);
    }
    CHECK(a.engine.torque_rpm() == b.engine.torque_rpm());
    CHECK(a.engine.torque_nm() == b.engine.torque_nm());
    CHECK(a.engine.bsfc_rpm() == b.engine.bsfc_rpm());
    CHECK(a.engine.bsfc_g_kwh() == b.engine.bsfc_g_kwh());
}

std::string default_json_text() { return fsae::io::car_spec_to_json(CarSpec::matlab_default()); }

std::string replace_once(std::string s, const std::string& from, const std::string& to) {
    const auto pos = s.find(from);
    REQUIRE(pos != std::string::npos);
    return s.replace(pos, from.size(), to);
}

}  // namespace

TEST_CASE("carro padrao identico ao initializeCar.m (car_params.csv)") {
    const Car car = Car::matlab_default();
    const auto ref = fsae::io::read_key_value_csv(reference_path("unit/car_params.csv"));

    // Campos do MATLAB que nao sao usados em nenhum calculo e nao foram portados.
    const std::vector<std::string> not_ported = {"brake_efficiency", "brake_density",
                                                 "brake_thickness", "brake_cp"};
    std::size_t matched = 0;
    for (const auto& [key, value] : matlab_view(car)) {
        INFO(key);
        REQUIRE(ref.count(key) == 1);
        CHECK(value == ref.at(key));  // igualdade bit a bit
        ++matched;
    }
    // Todo campo do MATLAB ou foi comparado ou esta na lista de nao portados.
    CHECK(matched + not_ported.size() == ref.size());
    for (const auto& k : not_ported) CHECK(ref.count(k) == 1);
}

TEST_CASE("mapas do motor do carro padrao (engine_eval.csv)") {
    const Car car = Car::matlab_default();
    const auto ref = load_reference("unit/engine_eval.csv");
    std::vector<double> torque, bsfc;
    for (double rpm : ref.column("rpm")) {
        torque.push_back(car.engine.torque(rpm));
        bsfc.push_back(car.engine.bsfc(rpm));
    }
    check_against_reference("torque", ref.column("torque_nm"), torque, 0.0);
    check_against_reference("bsfc", ref.column("bsfc_g_kwh"), bsfc, 0.0);
}

TEST_CASE("data/cars/default.json reproduz o carro padrao") {
    const Car from_file = fsae::io::load_car(std::string(FSAE_DATA_DIR) + "/cars/default.json");
    check_same_car(from_file, Car::matlab_default());
}

TEST_CASE("JSON: ida e volta preserva todos os valores") {
    const CarSpec spec = fsae::io::parse_car_spec(default_json_text());
    check_same_car(Car::from_spec(spec), Car::matlab_default());
}

TEST_CASE("JSON: erros sao detectados com mensagem clara") {
    const std::string ok = default_json_text();

    SUBCASE("campo desconhecido (erro de digitacao)") {
        const auto bad = replace_once(ok, "\"cg_height_m\"", "\"cg_heigth_m\"");
        CHECK_THROWS_WITH(fsae::io::parse_car_spec(bad), doctest::Contains("chassis.cg_heigth_m"));
    }
    SUBCASE("tipo errado") {
        const auto bad = replace_once(ok, "\"launch_rpm\": 8600.0", "\"launch_rpm\": \"8600\"");
        CHECK_THROWS_WITH(fsae::io::parse_car_spec(bad), doctest::Contains("launch_rpm"));
    }
    SUBCASE("sintaxe invalida") {
        CHECK_THROWS_WITH(fsae::io::parse_car_spec(ok.substr(0, ok.size() / 2)),
                          doctest::Contains("sintaxe"));
    }
    SUBCASE("rpm de troca com tamanho diferente do numero de marchas") {
        CarSpec s = CarSpec::matlab_default();
        s.upshift_rpm.pop_back();
        CHECK_THROWS_WITH(Car::from_spec(s), doctest::Contains("upshift_rpm"));
    }
    SUBCASE("massa negativa") {
        CarSpec s = CarSpec::matlab_default();
        s.car_mass_kg = -1.0;
        CHECK_THROWS_WITH(Car::from_spec(s), doctest::Contains("car_mass_kg"));
    }
}
