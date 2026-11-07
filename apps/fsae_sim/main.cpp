// fsae_sim — linha de comando do simulador de volta FSAE.

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "fsae/io/car_json.hpp"
#include "fsae/io/results.hpp"
#include "fsae/sim/lap.hpp"
#include "fsae/track/geometry.hpp"
#include "fsae/track/gpx.hpp"
#include "fsae/track/track.hpp"
#include "fsae/version.hpp"
#include "fsae/vehicle/car.hpp"

namespace fs = std::filesystem;

namespace {

const char* const kUsage = R"(fsae_sim - simulador de volta Formula SAE

Uso:
  fsae_sim run --track <arquivo|nome> [opcoes]   simula e grava os resultados
  fsae_sim list-tracks                           lista as pistas em data/tracks
  fsae_sim check-track <arquivo>                 valida uma pista
  fsae_sim gpx-slope <pista> <arquivo.gpx> <saida.csv>
                                                 preenche a inclinacao dos segmentos com a
                                                 altitude de um GPX (applyAltitudeFromGPX)
  fsae_sim convert-track <entrada> <saida.csv> [comentario]
                                                 converte pista (trackExcel/OptimumLap ou
                                                 matriz do MATLAB) para o CSV do projeto
  fsae_sim print-default-car                     imprime o carro padrao (initializeCar) em JSON
  fsae_sim check-car <arquivo.json>              valida um JSON de carro
  fsae_sim version

Opcoes do run:
  --track <arquivo|nome>   CSV da pista, ou nome de uma pista de data/tracks (ex.: autox)
  --car <arquivo.json>     carro (padrao: carro do initializeCar.m)
  --event <prova>          autocross | skidpad | acceleration | custom
                           (padrao: deduzido do nome da pista; autocross se nao for
                           skidpad/acceleration)
  --laps <n>               numero de voltas (padrao: 2 em autocross/custom, 1 nas outras)
  --start-segment <k>      a volta comeca no segmento k (padrao: 1)
  --out <pasta>            pasta dos resultados (padrao: results/<pista>)
  --no-files               nao grava arquivos, so mostra o resumo
  --json                   imprime o summary.json na saida padrao
  --verbose                relatorio trecho a trecho (como o MATLAB)
  --quiet                  nao imprime o resumo
  --repeat <n>             roda n vezes e mostra o tempo medio de execucao (benchmark)

Arquivos gravados em --out:
  telemetry.csv   historico completo (mesmas colunas do SimOut.History + lap)
  summary.json    tempos, consumo e maximos (SimOut.Results)
  track.csv       segmentos usados e velocidade maxima de cada curva
  geometry.csv    tracado X/Y/Z para graficos
  car.json        carro usado na simulacao

Graficos: tools/matlab/plotCppResult.m (MATLAB) ou tools/python/plot_results.py.
)";

[[noreturn]] void usage_error(const std::string& msg) {
    throw std::invalid_argument(msg + "\nUse \"fsae_sim help\" para ver as opcoes.");
}

/// Pastas `data` candidatas: $FSAE_DATA_DIR, ./data e subindo a partir do executável.
std::vector<fs::path> data_dirs(const char* argv0) {
    std::vector<fs::path> dirs;
    if (const char* env = std::getenv("FSAE_DATA_DIR")) dirs.emplace_back(env);
    dirs.emplace_back(fs::current_path() / "data");
    std::error_code ec;
    fs::path exe = fs::weakly_canonical(fs::absolute(argv0, ec), ec);
    if (!ec && exe.has_parent_path()) {
        fs::path dir = exe.parent_path();
        for (int up = 0; up < 5 && !dir.empty(); ++up) {
            dirs.push_back(dir / "data");
            if (dir == dir.parent_path()) break;
            dir = dir.parent_path();
        }
    }
    return dirs;
}

fs::path resolve_track(const std::string& arg, const char* argv0) {
    if (fs::exists(arg)) return arg;
    const std::string file = fs::path(arg).has_extension() ? arg : arg + ".csv";
    for (const auto& d : data_dirs(argv0)) {
        const fs::path p = d / "tracks" / file;
        if (fs::exists(p)) return p;
    }
    throw std::runtime_error("pista nao encontrada: \"" + arg +
                             "\" (passe o caminho do arquivo ou um nome de \"fsae_sim list-tracks\")");
}

struct RunArgs {
    std::string track;
    std::string car;
    std::optional<std::string> event;
    std::optional<int> laps;
    int start_segment = 1;
    std::string out;
    bool write_files = true;
    bool print_json = false;
    bool verbose = false;
    bool quiet = false;
    int repeat = 1;
};

int to_int(const std::string& s, const std::string& opt) {
    try {
        std::size_t pos = 0;
        const int v = std::stoi(s, &pos);
        if (pos == s.size()) return v;
    } catch (const std::exception&) {
    }
    usage_error(opt + " espera um numero inteiro, recebeu \"" + s + "\"");
}

RunArgs parse_run(const std::vector<std::string>& args) {
    RunArgs a;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string& o = args[i];
        auto value = [&]() -> const std::string& {
            if (i + 1 >= args.size()) usage_error(o + " precisa de um valor");
            return args[++i];
        };
        if (o == "--track") a.track = value();
        else if (o == "--car") a.car = value();
        else if (o == "--event") a.event = value();
        else if (o == "--laps") a.laps = to_int(value(), o);
        else if (o == "--start-segment") a.start_segment = to_int(value(), o);
        else if (o == "--out") a.out = value();
        else if (o == "--no-files") a.write_files = false;
        else if (o == "--json") a.print_json = true;
        else if (o == "--verbose") a.verbose = true;
        else if (o == "--quiet") a.quiet = true;
        else if (o == "--repeat") a.repeat = to_int(value(), o);
        else usage_error("opcao desconhecida: " + o);
    }
    if (a.track.empty()) usage_error("run precisa de --track");
    return a;
}

void print_summary(const fsae::sim::SimulationResult& r, const std::string& track,
                   const std::string& event, double elapsed_ms) {
    std::printf("\n----------------------------------------\n");
    std::printf("Pista: %s (%s, %d volta%s, %.2f m por volta)\n", track.c_str(), event.c_str(),
                r.laps, r.laps > 1 ? "s" : "", r.track_length);
    if (r.laps > 1) {
        for (int k = 0; k < r.laps; ++k) {
            char label[32];
            std::snprintf(label, sizeof label, "Volta %d (%s):", k + 1, k == 0 ? "partida" : "lancada");
            std::printf("%-21s%9.3f s\n", label, r.lap_times[static_cast<std::size_t>(k)]);
        }
        std::printf("Volta mais rapida:   %9.3f s\n", r.fastest_lap);
    }
    std::printf("Tempo total:         %9.3f s\n", r.total_time);
    std::printf("Velocidade maxima:   %9.2f km/h\n", r.max_velocity_kmh);
    std::printf("Acel. lateral max.:  %9.3f g\n", r.max_lat_g);
    std::printf("Temp. max. freio:    %9.1f C\n", r.max_brake_temp);
    std::printf("Consumo total:       %9.2f g\n", r.total_fuel_g);
    if (r.laps > 1) {
        for (int k = 0; k < r.laps; ++k) {
            char label[32];
            std::snprintf(label, sizeof label, "  consumo volta %d:", k + 1);
            std::printf("%-21s%9.2f g\n", label, r.lap_fuel_g[static_cast<std::size_t>(k)]);
        }
    }
    std::printf("----------------------------------------\n");
    std::printf("(simulado em %.1f ms)\n", elapsed_ms);
}

int cmd_run(const std::vector<std::string>& args, const char* argv0) {
    const RunArgs a = parse_run(args);

    const fs::path track_path = resolve_track(a.track, argv0);
    const auto track = fsae::track::load_track(track_path);
    const fsae::CarSpec car_spec =
        a.car.empty() ? fsae::CarSpec::matlab_default() : fsae::io::load_car_spec(a.car);
    const fsae::Car car = fsae::Car::from_spec(car_spec);

    fsae::sim::Event event = fsae::sim::Event::Autocross;
    if (a.event) {
        event = fsae::sim::parse_event(*a.event);
    } else if (track.name == "skidpad") {
        event = fsae::sim::Event::Skidpad;
    } else if (track.name == "acceleration") {
        event = fsae::sim::Event::Acceleration;
    }

    const auto segments = fsae::track::rotate_to_start(track.segments, a.start_segment);

    fsae::sim::SimulationOptions opt;
    opt.laps = a.laps.value_or(fsae::sim::default_laps(event));
    opt.log = a.verbose ? &std::cout : nullptr;

    if (a.repeat < 1) usage_error("--repeat deve ser >= 1");
    if (a.repeat > 1) opt.log = nullptr;
    fsae::sim::SimulationResult result;
    double best_ms = 1e300, total_ms = 0;
    for (int k = 0; k < a.repeat; ++k) {
        const auto t0 = std::chrono::steady_clock::now();
        result = fsae::sim::simulate(car, segments, opt);
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        best_ms = std::min(best_ms, ms);
        total_ms += ms;
    }
    const double elapsed_ms = total_ms / a.repeat;
    if (a.repeat > 1) {
        std::printf("Benchmark: %d execucoes, media %.3f ms, melhor %.3f ms (%.0f simulacoes/s)\n",
                    a.repeat, elapsed_ms, best_ms, 1000.0 / elapsed_ms);
    }

    fsae::io::RunInfo info;
    info.car_name = car.name;
    info.car_file = a.car;
    info.track_name = track.name;
    info.track_file = track_path.generic_string();
    info.event = fsae::sim::to_string(event);
    info.start_segment = a.start_segment;

    if (a.write_files) {
        const fs::path out = a.out.empty() ? fs::path("results") / track.name : fs::path(a.out);
        fsae::io::write_telemetry_csv(out / "telemetry.csv", result);
        fsae::io::write_summary_json(out / "summary.json", result, info);
        fsae::io::write_segments_csv(out / "track.csv", segments, result);
        fsae::io::write_geometry_csv(out / "geometry.csv", fsae::track::build_geometry(segments));
        fsae::io::save_car_spec(car_spec, out / "car.json");
        if (!a.quiet && !a.print_json) std::printf("\nResultados gravados em %s\n", out.generic_string().c_str());
    }
    if (a.print_json) {
        std::cout << fsae::io::summary_json(result, info);
    } else if (!a.quiet) {
        print_summary(result, track.name, info.event, elapsed_ms);
    }
    return 0;
}

int cmd_list_tracks(const char* argv0) {
    for (const auto& d : data_dirs(argv0)) {
        const fs::path dir = d / "tracks";
        if (!fs::is_directory(dir)) continue;
        std::printf("Pistas em %s:\n", dir.generic_string().c_str());
        for (const auto& e : fs::directory_iterator(dir)) {
            if (e.path().extension() != ".csv") continue;
            try {
                const auto t = fsae::track::load_track(e.path());
                std::printf("  %-28s %3zu segmentos  %8.2f m\n", t.name.c_str(), t.segments.size(),
                            t.length());
            } catch (const std::exception& ex) {
                std::printf("  %-28s ERRO: %s\n", e.path().stem().string().c_str(), ex.what());
            }
        }
        return 0;
    }
    std::printf("Nenhuma pasta data/tracks encontrada (defina FSAE_DATA_DIR).\n");
    return 1;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> args(argv + 1, argv + argc);
        const std::string cmd = args.empty() ? "help" : args.front();
        const std::vector<std::string> rest(args.begin() + (args.empty() ? 0 : 1), args.end());

        if (cmd == "run") return cmd_run(rest, argv[0]);
        if (cmd == "list-tracks") return cmd_list_tracks(argv[0]);
        if (cmd == "print-default-car") {
            std::cout << fsae::io::car_spec_to_json(fsae::CarSpec::matlab_default());
            return 0;
        }
        if (cmd == "check-car" && rest.size() == 1) {
            const fsae::Car car = fsae::io::load_car(rest[0]);
            std::cout << "OK: \"" << car.name << "\" (" << car.mass << " kg, " << car.gear_count()
                      << " marchas)\n";
            return 0;
        }
        if (cmd == "check-track" && rest.size() == 1) {
            const auto t = fsae::track::load_track(resolve_track(rest[0], argv[0]));
            std::size_t corners = 0;
            for (const auto& s : t.segments) corners += s.is_corner() ? 1 : 0;
            std::cout << "OK: \"" << t.name << "\" (" << t.segments.size() << " segmentos, "
                      << corners << " curvas, " << t.length() << " m)\n";
            return 0;
        }
        if (cmd == "gpx-slope" && rest.size() == 3) {
            const auto t = fsae::track::load_track(resolve_track(rest[0], argv[0]));
            const auto pts = fsae::track::load_gpx(rest[1]);
            const auto r = fsae::track::apply_gpx_altitude(t.segments, pts);
            std::printf("%-4s %10s %10s %10s %10s %12s\n", "seg", "L (m)", "z ini (m)", "z fim (m)",
                        "dz (m)", "slope (rad)");
            for (std::size_t i = 0; i < r.segments.size(); ++i) {
                std::printf("%-4zu %10.3f %10.3f %10.3f %10.4f %12.6f\n", i + 1, r.segments[i].length,
                            r.alt_start[i], r.alt_end[i], r.delta_z[i], r.segments[i].slope);
            }
            std::printf("Erro de fechamento do GPX (distribuido): %.4f m (%zu pontos)\n",
                        r.z_error_total, pts.size());
            fsae::track::Track out{t.name, r.segments};
            fsae::track::save_track(out, rest[2],
                                    "Inclinacao [rad] calculada de " +
                                        fs::path(rest[1]).filename().string() + " (fsae_sim gpx-slope)");
            std::printf("Gravado %s\n", rest[2].c_str());
            return 0;
        }
        if (cmd == "convert-track" && (rest.size() == 2 || rest.size() == 3)) {
            const auto t = fsae::track::load_track(rest[0]);
            fsae::track::save_track(t, rest[1], rest.size() == 3 ? rest[2] : "");
            std::cout << "Gravado " << rest[1] << " (" << t.segments.size() << " segmentos)\n";
            return 0;
        }
        if (cmd == "version" || cmd == "--version") {
            std::cout << fsae::version() << "\n";
            return 0;
        }
        if (cmd == "help" || cmd == "--help" || cmd == "-h") {
            std::cout << kUsage;
            return 0;
        }
        std::cerr << "Comando desconhecido ou argumentos invalidos: " << cmd << "\n\n" << kUsage;
        return 2;
    } catch (const std::invalid_argument& e) {
        std::cerr << "Erro: " << e.what() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << "\n";
        return 1;
    }
}
