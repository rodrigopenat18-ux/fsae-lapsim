#include "fsae/io/results.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "fsae/version.hpp"

namespace fsae::io {

namespace {

std::ofstream open_out(const std::filesystem::path& path) {
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Nao foi possivel gravar " + path.string());
    return out;
}

/// NaN/inf não existem em JSON: vira null.
nlohmann::ordered_json json_number(double v) {
    if (!std::isfinite(v)) return nullptr;
    return v;
}

nlohmann::ordered_json json_array(const std::vector<double>& v) {
    auto a = nlohmann::ordered_json::array();
    for (double x : v) a.push_back(json_number(x));
    return a;
}

}  // namespace

std::string format_number(double v) {
    if (std::isnan(v)) return "nan";
    if (std::isinf(v)) return v > 0 ? "inf" : "-inf";
    char buf[64];
    const auto r = std::to_chars(buf, buf + sizeof buf, v);
    return std::string(buf, r.ptr);
}

void write_telemetry_csv(const std::filesystem::path& path, const sim::SimulationResult& result) {
    const auto& h = result.history;
    auto out = open_out(path);
    out << "time,distance,velocity,gear,acceleration,rpm,lateral_accel,drag,downforce,"
           "rolling_resistance,slope_force,engine_force,brake_force,brake_temp,h_coef,"
           "fuel_consumed_g,P_conv,P_rad,P_cond,load_front_dist,lap\n";
    std::size_t lap = 0;
    std::string line;
    for (std::size_t i = 0; i < h.size(); ++i) {
        while (lap + 1 < result.lap_start_index.size() && i >= result.lap_start_index[lap + 1]) ++lap;
        line.clear();
        const double row[] = {h.time[i],          h.distance[i],          h.velocity[i],
                              static_cast<double>(h.gear[i]),
                              h.acceleration[i],  h.rpm[i],               h.lateral_accel[i],
                              h.drag[i],          h.downforce[i],         h.rolling_resistance[i],
                              h.slope_force[i],   h.engine_force[i],      h.brake_force[i],
                              h.brake_temp[i],    h.h_coef[i],            h.fuel_consumed_g[i],
                              h.p_conv[i],        h.p_rad[i],             h.p_cond[i],
                              h.load_front_dist[i]};
        for (double x : row) {
            line += format_number(x);
            line += ',';
        }
        line += std::to_string(lap + 1);
        line += '\n';
        out << line;
    }
}

std::string summary_json(const sim::SimulationResult& r, const RunInfo& info) {
    nlohmann::ordered_json j;
    j["fsae_lapsim_version"] = version();
    j["car"] = {{"name", info.car_name}, {"file", info.car_file}};
    j["track"] = {{"name", info.track_name},
                  {"file", info.track_file},
                  {"length_m", r.track_length},
                  {"start_segment", info.start_segment}};
    j["event"] = info.event;
    j["laps"] = r.laps;
    j["results"] = {
        {"total_time_s", json_number(r.total_time)},
        {"lap_times_s", json_array(r.lap_times)},
        {"fastest_lap_s", json_number(r.fastest_lap)},
        {"max_velocity_kmh", json_number(r.max_velocity_kmh)},
        {"max_lat_g", json_number(r.max_lat_g)},
        {"max_brake_temp_c", json_number(r.max_brake_temp)},
        {"total_fuel_g", json_number(r.total_fuel_g)},
        {"lap_fuel_g", json_array(r.lap_fuel_g)},
    };
    // Mesmos nomes do SimOut.Results do MATLAB (Lap2_Time = 0 com uma volta só)
    j["matlab_results"] = {
        {"TotalTime", json_number(r.total_time)},
        {"MaxVelocity", json_number(r.max_velocity_kmh)},
        {"MaxLatG", json_number(r.max_lat_g)},
        {"MaxBrakeTemp", json_number(r.max_brake_temp)},
        {"TotalFuel_g", json_number(r.total_fuel_g)},
        {"Lap1_Time", json_number(r.lap_times[0])},
        {"Lap2_Time", json_number(r.laps >= 2 ? r.lap_times[1] : 0.0)},
        {"FastestLap", json_number(r.fastest_lap)},
    };
    j["samples"] = r.history.size();
    return j.dump(2) + "\n";
}

void write_summary_json(const std::filesystem::path& path, const sim::SimulationResult& result,
                        const RunInfo& info) {
    auto out = open_out(path);
    out << summary_json(result, info);
}

void write_segments_csv(const std::filesystem::path& path,
                        const std::vector<track::Segment>& segments,
                        const sim::SimulationResult& result) {
    auto out = open_out(path);
    out << "segment,length_m,radius_m,slope,type,corner_speed_ms,v_grip_ms,v_tip_ms,limited_by_tipping\n";
    for (std::size_t k = 0; k < segments.size(); ++k) {
        const auto& s = segments[k];
        const auto& c = result.corner_speeds[k];
        out << k + 1 << ',' << format_number(s.length) << ',' << format_number(s.radius) << ','
            << format_number(s.slope) << ',' << track::to_string(s.type) << ','
            << format_number(c.v_max) << ',' << format_number(c.v_grip) << ','
            << format_number(c.v_tip) << ',' << (s.is_corner() && c.limited_by_tipping() ? 1 : 0)
            << '\n';
    }
}

void write_geometry_csv(const std::filesystem::path& path, const track::TrackGeometry& g) {
    auto out = open_out(path);
    out << "x,y,z,heading\n";
    for (std::size_t i = 0; i < g.size(); ++i) {
        out << format_number(g.x[i]) << ',' << format_number(g.y[i]) << ',' << format_number(g.z[i])
            << ',' << format_number(g.heading[i]) << '\n';
    }
}

}  // namespace fsae::io
