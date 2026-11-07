#include <doctest/doctest.h>

#include <cmath>
#include <sstream>
#include <string>
#include <vector>

#include "arquivos/csv.hpp"
#include "simulacao/lap.hpp"
#include "pista/track.hpp"
#include "carro/car.hpp"
#include "support/reference.hpp"

using fsae::Car;
using fsae::test::check_against_reference;
using fsae::test::load_reference;
using fsae::test::reference_path;
namespace sim = fsae::sim;
namespace tr = fsae::track;

namespace {

/// Folga para libm de outras plataformas (ver validacao/gabarito/README.md). As grandezas
/// térmicas (h, P_conv, P_rad) herdam a diferença de 1 bit do `pow` do MATLAB.
constexpr double kLibm = 1e-12;

const Car& car() {
    static const Car c = Car::matlab_default();
    return c;
}

struct Case {
    const char* reference;
    const char* file;
    sim::Event event;
};

const Case kCases[] = {
    {"autox", "autox.csv", sim::Event::Autocross},
    {"skidpad", "skidpad.csv", sim::Event::Skidpad},
    {"acceleration", "acceleration.csv", sim::Event::Acceleration},
    {"autox2024v1", "autox_2024_v1.csv", sim::Event::Custom},
    {"autox2024v1_rev", "autox_2024_v1_reversed.csv", sim::Event::Custom},
};

sim::SimulationResult run(const Case& c, std::ostream* log = nullptr) {
    const auto t = tr::load_track(std::string(FSAE_DATA_DIR) + "/pistas/" + c.file);
    sim::SimulationOptions opt;
    opt.laps = sim::default_laps(c.event);
    opt.log = log;
    return sim::simulate(car(), t.segments, opt);
}

std::vector<double> as_double(const std::vector<int>& v) { return {v.begin(), v.end()}; }

}  // namespace

TEST_CASE("volta completa nas 5 pistas: historico e resultados iguais ao MATLAB") {
    for (const auto& c : kCases) {
        const std::string name = c.reference;
        CAPTURE(name);
        const std::string dir = std::string("tracks/") + c.reference + "/";
        const auto r = run(c);
        const auto& h = r.history;

        // --- SimOut.Results ---
        const auto res = fsae::io::read_key_value_csv(reference_path(dir + "results.csv"));
        CHECK(r.total_time == res.at("TotalTime"));
        CHECK(r.max_velocity_kmh == res.at("MaxVelocity"));
        CHECK(r.max_lat_g == res.at("MaxLatG"));
        CHECK(r.max_brake_temp == doctest::Approx(res.at("MaxBrakeTemp")).epsilon(1e-12));
        CHECK(r.total_fuel_g == res.at("TotalFuel_g"));
        CHECK(r.lap_times[0] == res.at("Lap1_Time"));
        if (r.laps == 2) {
            CHECK(r.lap_times[1] == res.at("Lap2_Time"));
        } else {
            CHECK(res.at("Lap2_Time") == 0.0);  // MATLAB grava 0 quando há uma volta só
        }
        CHECK(r.fastest_lap == res.at("FastestLap"));
        std::string laps_text = std::to_string(r.lap_times[0]);
        if (r.laps == 2) laps_text += " / " + std::to_string(r.lap_times[1]);
        MESSAGE(name << ": total " << r.total_time << " s, voltas " << laps_text);

        // --- SimOut.History ---
        const auto ref = load_reference(dir + "history.csv");
        REQUIRE(h.size() == ref.rows());
        const std::string p = dir;
        check_against_reference(p + "time", ref.column("time"), h.time, 0.0);
        check_against_reference(p + "distance", ref.column("distance"), h.distance, kLibm);
        check_against_reference(p + "velocity", ref.column("velocity"), h.velocity, kLibm);
        check_against_reference(p + "gear", ref.column("gear"), as_double(h.gear), 0.0);
        check_against_reference(p + "acceleration", ref.column("acceleration"), h.acceleration, kLibm);
        check_against_reference(p + "rpm", ref.column("rpm"), h.rpm, kLibm);
        check_against_reference(p + "lateral_accel", ref.column("lateral_accel"), h.lateral_accel, kLibm);
        check_against_reference(p + "drag", ref.column("drag"), h.drag, kLibm);
        check_against_reference(p + "downforce", ref.column("downforce"), h.downforce, kLibm);
        check_against_reference(p + "rolling_resistance", ref.column("rolling_resistance"),
                                h.rolling_resistance, kLibm);
        check_against_reference(p + "slope_force", ref.column("slope_force"), h.slope_force, kLibm);
        check_against_reference(p + "engine_force", ref.column("engine_force"), h.engine_force, kLibm);
        check_against_reference(p + "brake_force", ref.column("brake_force"), h.brake_force, kLibm);
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

TEST_CASE("numero de voltas como parametro") {
    const auto t = tr::load_track(std::string(FSAE_DATA_DIR) + "/pistas/autox.csv");
    sim::SimulationOptions opt;

    opt.laps = 2;
    const auto two = sim::simulate(car(), t.segments, opt);
    opt.laps = 4;
    const auto four = sim::simulate(car(), t.segments, opt);

    REQUIRE(four.lap_times.size() == 4);
    // As duas primeiras voltas não dependem do que vem depois.
    CHECK(four.lap_times[0] == two.lap_times[0]);
    // Voltas lançadas: mais rápidas que a primeira e parecidas entre si. Não são
    // idênticas porque a reta final se funde com a reta de largada e a divisão das
    // voltas é pela distância, com o "overshoot" das retas (docs/problemas_conhecidos.md, item 3):
    // alguns centésimos migram de uma volta para a outra.
    for (int k = 1; k < 4; ++k) {
        CHECK(four.lap_times[k] < four.lap_times[0]);
        CHECK(std::abs(four.lap_times[k] - two.lap_times[1]) < 0.3);
    }
    double sum = 0;
    for (double l : four.lap_times) sum += l;
    CHECK(sum == doctest::Approx(four.total_time).epsilon(1e-12));
    CHECK(four.lap_start_index[0] == 0);
    CHECK(four.lap_start_index[1] < four.lap_start_index[2]);
    CHECK(four.lap_start_index[2] < four.lap_start_index[3]);

    opt.laps = 0;
    CHECK_THROWS((void)sim::simulate(car(), t.segments, opt));
}

TEST_CASE("relatorio trecho a trecho (verbose)") {
    std::ostringstream log;
    (void)run(kCases[1], &log);  // skidpad
    const std::string s = log.str();
    CHECK(s.find("INICIANDO SIMULACAO: 2 TRECHOS TOTAIS") != std::string::npos);
    CHECK(s.find("TRECHO 01 (RETA)") != std::string::npos);
    CHECK(s.find("TRECHO 02 (CURVA)") != std::string::npos);
}

TEST_CASE("prova: voltas padrao e nomes") {
    CHECK(sim::default_laps(sim::Event::Autocross) == 2);
    CHECK(sim::default_laps(sim::Event::Custom) == 2);
    CHECK(sim::default_laps(sim::Event::Skidpad) == 1);
    CHECK(sim::default_laps(sim::Event::Acceleration) == 1);
    CHECK(sim::parse_event("autox") == sim::Event::Autocross);
    CHECK(sim::parse_event("skidpad") == sim::Event::Skidpad);
    CHECK_THROWS((void)sim::parse_event("endurance"));
}
