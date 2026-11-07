#include "Simulation/lap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <string>

#include "Control/gearbox.hpp"
#include "Math/interp.hpp"
#include "Physics/brake_thermal.hpp"
#include "Physics/load_transfer.hpp"
#include "Physics/powertrain.hpp"
#include "Physics/resistances.hpp"
#include "Simulation/straight.hpp"

namespace fsae::sim {

int default_laps(Event event) noexcept {
    return (event == Event::Autocross || event == Event::Custom) ? 2 : 1;
}

const char* to_string(Event event) noexcept {
    switch (event) {
        case Event::Autocross: return "autocross";
        case Event::Skidpad: return "skidpad";
        case Event::Acceleration: return "acceleration";
        case Event::Custom: break;
    }
    return "custom";
}

Event parse_event(const std::string& name) {
    if (name == "autocross" || name == "autox") return Event::Autocross;
    if (name == "skidpad") return Event::Skidpad;
    if (name == "acceleration" || name == "accel") return Event::Acceleration;
    if (name == "custom") return Event::Custom;
    throw std::invalid_argument("prova desconhecida: \"" + name +
                                "\" (use autocross, skidpad, acceleration ou custom)");
}

void Telemetry::reserve(std::size_t n) {
    for (auto* v : {&time, &distance, &velocity, &acceleration, &rpm, &lateral_accel, &drag,
                    &downforce, &rolling_resistance, &slope_force, &engine_force, &brake_force,
                    &brake_temp, &h_coef, &fuel_consumed_g, &p_conv, &p_rad, &p_cond,
                    &load_front_dist}) {
        v->reserve(n);
    }
    gear.reserve(n);
}

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

/// printf para o log (só formata se houver log).
template <typename... Args>
void logf(std::ostream* log, const char* fmt, Args... args) {
    if (log == nullptr) return;
    char buf[512];
    std::snprintf(buf, sizeof buf, fmt, args...);
    *log << buf;
}

/// Acrescenta uma reta ao histórico (pula a 1ª amostra, que repete o estado atual).
void append_straight(Telemetry& h, const StraightHistory& s, double fuel_offset) {
    const double base_t = h.time.back();
    const double base_d = h.distance.back();
    for (std::size_t k = 1; k < s.size(); ++k) {
        h.time.push_back(s.time[k] + base_t);
        h.distance.push_back(s.position[k] + base_d);
        h.velocity.push_back(s.velocity[k]);
        h.gear.push_back(s.gear[k]);
        h.acceleration.push_back(s.acceleration[k]);
        h.rpm.push_back(s.rpm[k]);
        h.lateral_accel.push_back(0.0);
        h.drag.push_back(s.drag[k]);
        h.downforce.push_back(s.downforce[k]);
        h.rolling_resistance.push_back(s.rolling_resistance[k]);
        h.slope_force.push_back(s.slope_force[k]);
        h.engine_force.push_back(s.engine_force[k]);
        h.brake_force.push_back(s.brake_force[k]);
        h.brake_temp.push_back(s.brake_temp[k]);
        h.h_coef.push_back(s.h_coef[k]);
        h.fuel_consumed_g.push_back(s.fuel_consumed_g[k] + fuel_offset);
        h.p_conv.push_back(s.p_conv[k]);
        h.p_rad.push_back(s.p_rad[k]);
        h.p_cond.push_back(s.p_cond[k]);
        h.load_front_dist.push_back(s.load_front_dist[k]);
    }
}

/// Estado que atravessa os trechos.
struct LoopState {
    double total_time = 0;
    double current_vel = 0;
    double current_temp = 25;
    double total_fuel_g = 0;
};

/// Curva a velocidade constante (o carro entra com min(v_atual, v_max) e mantém).
void run_corner(const Car& car, const track::Segment& seg, double max_v, double slope,
                int lap, int seg_in_lap, LoopState& st, Telemetry& h, std::ostream* log) {
    const double L = seg.length;
    const double R = seg.radius;
    const double actual_v = std::min(st.current_vel, max_v);

    logf(log, "\n[VOLTA %d] TRECHO %02d (CURVA) | R: %.1fm | L: %.1fm\n", lap, seg_in_lap, R, L);
    logf(log, "      > Alvo de Seguranca: %.2f km/h\n", max_v * 3.6);
    logf(log, "      > Entrada Real:      %.2f km/h\n", st.current_vel * 3.6);
    if (st.current_vel > (max_v + 0.1)) {
        logf(log, "      [!] ALERTA: O carro nao freou o suficiente! Entrou acima do limite de "
                  "tombamento.\n");
    }

    const double time_seg = L / std::max(actual_v, 0.1);
    if (time_seg > 0) {
        const auto nP = static_cast<std::size_t>(std::max(2.0, std::floor(time_seg / 0.05)));
        const auto t_add = math::linspace(0, time_seg, nP + 1);
        const auto d_add = math::linspace(0, L, nP + 1);
        const double base_t = h.time.back();
        const double base_d = h.distance.back();

        const int gear = control::find_initial_gear(car, actual_v);
        const double rpm = physics::engine_rpm(car, actual_v, gear);
        const double sign = seg.type == track::SegmentType::Left ? -1.0 : 1.0;  // (-1)^(type==1)
        const double lat_accel_ms2 = (actual_v * actual_v / R) * sign;
        const double lat_accel_g = lat_accel_ms2 / 9.81;

        const double f_drag = physics::drag(car, actual_v);
        const double f_down = physics::downforce(car, actual_v);
        const double f_slope = physics::slope_force(car, slope);
        const double f_roll = physics::tyre_resistance(car, actual_v, slope, f_down);
        const double f_engine = f_drag + f_roll + f_slope;

        const double dt_curve = time_seg / static_cast<double>(nP);

        const auto lt = physics::load_transfer(car, 0, lat_accel_g, slope, actual_v);
        const auto& n = lt.normal_n;
        const double front_dist_val = (n[0] + n[1]) / (n[0] + n[1] + n[2] + n[3]) * 100;
        const double ratio_val = front_dist_val / 100;

        const double pot_kw = std::max(0.0, (f_engine * actual_v) / 1000);
        const double bsfc = car.engine.bsfc(rpm);
        const double fuel_step = (bsfc * pot_kw * dt_curve) / 3600;

        for (std::size_t j = 1; j <= nP; ++j) {
            const auto bt = physics::brake_temperature(car, actual_v, 0, st.current_temp, dt_curve,
                                                       ratio_val);
            st.current_temp = bt.new_temp;
            st.total_fuel_g = st.total_fuel_g + fuel_step;

            h.time.push_back(base_t + t_add[j]);
            h.distance.push_back(base_d + d_add[j]);
            h.velocity.push_back(actual_v);
            h.gear.push_back(gear);
            h.acceleration.push_back(0.0);
            h.rpm.push_back(rpm);
            h.lateral_accel.push_back(lat_accel_ms2);
            h.drag.push_back(f_drag);
            h.downforce.push_back(f_down);
            h.rolling_resistance.push_back(f_roll);
            h.slope_force.push_back(f_slope);
            h.engine_force.push_back(f_engine);
            h.brake_force.push_back(0.0);
            h.brake_temp.push_back(st.current_temp);
            h.h_coef.push_back(bt.h);
            h.fuel_consumed_g.push_back(st.total_fuel_g);
            h.p_conv.push_back(bt.p_conv);
            h.p_rad.push_back(bt.p_rad);
            h.p_cond.push_back(bt.p_cond);
            h.load_front_dist.push_back(front_dist_val);
        }
    }
    st.current_vel = actual_v;
    st.total_time = st.total_time + time_seg;
}

/// Pós-processamento: máximos e separação das voltas.
void summarize(SimulationResult& r, double lap_length) {
    const auto& h = r.history;
    r.max_velocity_kmh = *std::max_element(h.velocity.begin(), h.velocity.end()) * 3.6;
    double max_lat = 0;
    for (double a : h.lateral_accel) max_lat = std::max(max_lat, std::abs(a));
    r.max_lat_g = max_lat / 9.81;
    r.max_brake_temp = *std::max_element(h.brake_temp.begin(), h.brake_temp.end());
    r.total_fuel_g = h.fuel_consumed_g.back();

    const auto n = static_cast<std::size_t>(r.laps);
    r.lap_times.assign(n, kNaN);
    r.lap_fuel_g.assign(n, kNaN);
    r.lap_start_index.assign(n, h.size());
    r.lap_start_index[0] = 0;

    if (n == 1) {
        r.lap_times[0] = r.total_time;
        r.lap_fuel_g[0] = r.total_fuel_g;
        r.fastest_lap = r.total_time;
        return;
    }

    // Fim da volta k: primeira amostra com distância ≥ k·L (MATLAB: find(distance >= L, 1)).
    double prev_time = 0, prev_fuel = 0;
    std::size_t search_from = 0;
    bool closed = true;
    for (std::size_t k = 1; k < n && closed; ++k) {
        const double threshold = k == 1 ? lap_length : static_cast<double>(k) * lap_length;
        std::size_t idx = h.size();
        for (std::size_t i = search_from; i < h.size(); ++i) {
            if (h.distance[i] >= threshold) {
                idx = i;
                break;
            }
        }
        if (idx == h.size()) {
            // A volta não fechou: o restante do tempo fica nesta volta (fallback do MATLAB).
            r.lap_times[k - 1] = r.total_time - prev_time;
            r.lap_fuel_g[k - 1] = r.total_fuel_g - prev_fuel;
            closed = false;
            break;
        }
        r.lap_times[k - 1] = h.time[idx] - prev_time;
        r.lap_fuel_g[k - 1] = h.fuel_consumed_g[idx] - prev_fuel;
        prev_time = h.time[idx];
        prev_fuel = h.fuel_consumed_g[idx];
        r.lap_start_index[k] = idx;
        search_from = idx;
    }
    if (closed) {
        r.lap_times[n - 1] = r.total_time - prev_time;
        r.lap_fuel_g[n - 1] = r.total_fuel_g - prev_fuel;
    }

    double fastest = kInf;
    for (std::size_t k = 1; k < n; ++k) {
        if (std::isfinite(r.lap_times[k])) fastest = std::min(fastest, r.lap_times[k]);
    }
    r.fastest_lap = std::isfinite(fastest) ? fastest : r.total_time;
}

}  // namespace

SimulationResult simulate(const Car& car, const std::vector<track::Segment>& lap_segments,
                          const SimulationOptions& options) {
    if (lap_segments.empty()) throw std::invalid_argument("pista sem segmentos");
    if (options.laps < 1) throw std::invalid_argument("numero de voltas deve ser >= 1");
    std::ostream* log = options.log;

    SimulationResult r;
    r.laps = options.laps;

    // --- Preparação (initializeTrack + repetição das voltas) ---
    r.corner_speeds = track::corner_speeds(car, lap_segments);
    for (std::size_t k = 0; k < lap_segments.size(); ++k) {
        const auto& c = r.corner_speeds[k];
        if (lap_segments[k].is_corner() && c.limited_by_tipping()) {
            logf(log,
                 "\n[AVISO] CURVA %zu: Roda levantou (Carga <= 0)!\n"
                 "      Velocidade de Grip:        %.2f km/h\n"
                 "      Velocidade por Tombamento: %.2f km/h (LIMITANTE)\n",
                 k + 1, c.v_grip * 3.6, c.v_tip * 3.6);
        }
    }

    const std::size_t per_lap = lap_segments.size();
    const std::size_t total_segments = per_lap * static_cast<std::size_t>(options.laps);
    std::vector<track::Segment> segments;
    std::vector<double> corner_v;
    segments.reserve(total_segments);
    corner_v.reserve(total_segments);
    for (int lap = 0; lap < options.laps; ++lap) {
        for (std::size_t k = 0; k < per_lap; ++k) {
            segments.push_back(lap_segments[k]);
            corner_v.push_back(r.corner_speeds[k].v_max);
        }
    }
    // MATLAB: sum(segments(:,2)) de uma volta
    double lap_length = 0;
    for (const auto& s : lap_segments) lap_length += s.length;
    r.track_length = lap_length;

    const bool multi_lap = options.laps > 1;
    logf(log, "\n=======================================================\n");
    logf(log, " INICIANDO SIMULACAO: %zu TRECHOS TOTAIS\n", total_segments);
    logf(log, " Velocidade de Partida: %.1f km/h\n", options.initial_velocity * 3.6);
    logf(log, "=======================================================\n");

    // --- Estado inicial ---
    LoopState st;
    st.current_vel = options.initial_velocity;
    st.current_temp = options.initial_brake_temp;

    Telemetry& h = r.history;
    h.reserve(8192);
    int gear0 = 1;
    double rpm0 = car.engine.rpm_min();
    if (st.current_vel > 1) {
        gear0 = control::find_initial_gear(car, st.current_vel);
        rpm0 = physics::engine_rpm(car, st.current_vel, gear0);
    }
    h.time.push_back(0);
    h.distance.push_back(0);
    h.velocity.push_back(st.current_vel);
    h.gear.push_back(gear0);
    h.acceleration.push_back(0);
    h.rpm.push_back(rpm0);
    h.lateral_accel.push_back(0);
    h.drag.push_back(0);
    h.downforce.push_back(0);
    h.rolling_resistance.push_back(0);
    h.slope_force.push_back(0);
    h.engine_force.push_back(0);
    h.brake_force.push_back(0);
    h.brake_temp.push_back(st.current_temp);
    h.h_coef.push_back(0);
    h.fuel_consumed_g.push_back(0);
    h.p_conv.push_back(0);
    h.p_rad.push_back(0);
    h.p_cond.push_back(0);
    h.load_front_dist.push_back((1 - car.weight_dist_rear_percent / 100) * 100);

    // --- Laço principal ---
    std::size_t i = 0;
    while (i < total_segments) {
        const auto& seg = segments[i];
        const int lap = multi_lap ? static_cast<int>(i / per_lap) + 1 : 1;
        const int seg_in_lap = static_cast<int>(multi_lap ? i % per_lap : i) + 1;
        const double slope = seg.slope;

        if (seg.radius != 0) {
            run_corner(car, seg, corner_v[i], slope, lap, seg_in_lap, st, h, log);
            ++i;
            continue;
        }

        // Reta: funde com as retas seguintes (inclinação média ponderada pelo comprimento)
        double temp_L = seg.length;
        double slope_accum = slope * seg.length;
        std::size_t count_merge = 0;
        for (std::size_t k = i + 1; k < total_segments; ++k) {
            if (segments[k].radius != 0) break;
            ++count_merge;
            temp_L = temp_L + segments[k].length;
            slope_accum = slope_accum + (segments[k].slope * segments[k].length);
        }

        double straight_slope = slope;
        if (count_merge > 0) {
            straight_slope = slope_accum / temp_L;
            logf(log, "\n[VOLTA %d] TRECHO %02d (RETA FUNDIDA: %zu segs) | Total: %.1fm\n", lap,
                 seg_in_lap, count_merge + 1, temp_L);
        } else {
            logf(log, "\n[VOLTA %d] TRECHO %02d (RETA) | Comprimento: %.1fm\n", lap, seg_in_lap,
                 temp_L);
        }
        logf(log, "      > Entrada: %.1f km/h\n", st.current_vel * 3.6);

        const double target_v = track::next_corner_speed(segments, corner_v, i + count_merge);
        const auto res = simulate_straight(car, temp_L, st.current_vel, target_v, straight_slope,
                                           st.current_temp);
        if (res.history.size() > 1) {
            append_straight(h, res.history, st.total_fuel_g);
            st.total_fuel_g = h.fuel_consumed_g.back();
            st.current_vel = res.history.velocity.back();
            st.current_temp = res.history.brake_temp.back();
        }

        if (count_merge > 0) {
            logf(log, "      > Alvo Final: %.1f km/h\n", target_v * 3.6);
            logf(log, "      > SAIDA COMBINADA: %.1f km/h | Tempo Total: %.3fs\n",
                 st.current_vel * 3.6, res.total_time);
        } else {
            logf(log, "      > SAIDA: %.1f km/h | Tempo: %.3fs\n", st.current_vel * 3.6,
                 res.total_time);
        }
        st.total_time = st.total_time + res.total_time;
        i += count_merge + 1;
    }

    r.total_time = st.total_time;
    summarize(r, lap_length);
    return r;
}

}  // namespace fsae::sim
