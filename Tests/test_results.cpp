#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "IO_Tools/csv.hpp"
#include "IO_Tools/results.hpp"
#include "Simulation/lap.hpp"
#include "Tracks/geometry.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"
#include "support/reference.hpp"

namespace fs = std::filesystem;
using fsae::test::check_against_reference;
using fsae::test::load_reference;

namespace {

fs::path temp_dir(const std::string& name) {
    const fs::path d = fs::temp_directory_path() / ("fsae_lapsim_test_" + name);
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}

fsae::sim::SimulationResult run_autox() {
    const auto t = fsae::track::load_track(std::string(FSAE_DATA_DIR) + "/Tracks/autox.csv");
    fsae::sim::SimulationOptions opt;
    opt.laps = 2;
    return fsae::sim::simulate(fsae::Car::matlab_default(), t.segments, opt);
}

}  // namespace

TEST_CASE("format_number preserva o valor exato") {
    for (double v : {0.1, 1.0 / 3.0, 4.2499999999999538, 1e-300, 123456789.123456789, -2.5e-7}) {
        CHECK(std::strtod(fsae::io::format_number(v).c_str(), nullptr) == v);
    }
    CHECK(fsae::io::format_number(0.1) == "0.1");
    CHECK(fsae::io::format_number(std::numeric_limits<double>::infinity()) == "inf");
    CHECK(fsae::io::format_number(std::nan("")) == "nan");
}

TEST_CASE("telemetry.csv relido e igual ao historico do MATLAB") {
    const auto r = run_autox();
    const fs::path dir = temp_dir("telemetry");
    fsae::io::write_telemetry_csv(dir / "telemetry.csv", r);

    const auto t = fsae::io::read_numeric_csv(dir / "telemetry.csv");
    const auto ref = load_reference("tracks/autox/history.csv");
    REQUIRE(t.rows() == ref.rows());
    // Colunas que batem bit a bit com o MATLAB continuam idênticas depois de gravar e reler.
    for (const char* col : {"time", "distance", "velocity", "gear", "rpm", "drag", "engine_force",
                            "brake_force", "fuel_consumed_g", "lateral_accel"}) {
        check_against_reference(std::string("telemetry ") + col, ref.column(col), t.column(col), 0.0);
    }
    // Coluna lap: 1 até o fim da volta 1, depois 2.
    const auto& lap = t.column("lap");
    CHECK(lap.front() == 1.0);
    CHECK(lap.back() == 2.0);
    CHECK(lap[r.lap_start_index[1] - 1] == 1.0);
    CHECK(lap[r.lap_start_index[1]] == 2.0);
    fs::remove_all(dir);
}

TEST_CASE("summary.json com os resultados") {
    const auto r = run_autox();
    fsae::io::RunInfo info;
    info.car_name = "padrao";
    info.track_name = "autox";
    info.event = "autocross";
    const auto j = nlohmann::json::parse(fsae::io::summary_json(r, info));
    CHECK(j["laps"] == 2);
    CHECK(j["results"]["total_time_s"].get<double>() == r.total_time);
    CHECK(j["results"]["lap_times_s"][1].get<double>() == r.lap_times[1]);
    CHECK(j["matlab_results"]["Lap2_Time"].get<double>() == 45.500610291853917);
    CHECK(j["matlab_results"]["TotalTime"].get<double>() == 92.992466883276208);
    CHECK(j["track"]["name"] == "autox");
}

TEST_CASE("track.csv e geometry.csv") {
    const auto t = fsae::track::load_track(std::string(FSAE_DATA_DIR) + "/Tracks/skidpad.csv");
    const auto r = fsae::sim::simulate(fsae::Car::matlab_default(), t.segments, {});
    const fs::path dir = temp_dir("segments");
    fsae::io::write_segments_csv(dir / "track.csv", t.segments, r);
    fsae::io::write_geometry_csv(dir / "geometry.csv", fsae::track::build_geometry(t.segments));

    std::string text;
    {
        std::ifstream in(dir / "track.csv");
        std::stringstream ss;
        ss << in.rdbuf();
        text = ss.str();
    }
    CHECK(text.find("2,51.5,8.2,0,left,") != std::string::npos);

    const auto g = fsae::io::read_numeric_csv(dir / "geometry.csv");
    CHECK(g.rows() == 2 * fsae::track::kPointsPerSegment);
    fs::remove_all(dir);
}
