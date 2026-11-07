#include <exception>
#include <iostream>
#include <string>

#include "fsae/io/car_json.hpp"
#include "fsae/track/track.hpp"
#include "fsae/version.hpp"
#include "fsae/vehicle/car.hpp"

namespace {

void print_usage() {
    std::cout << "fsae_sim " << fsae::version() << " - simulador de volta FSAE\n\n"
              << "Uso:\n"
              << "  fsae_sim --print-default-car              imprime o carro padrao (initializeCar) em JSON\n"
              << "  fsae_sim --check-car <arquivo.json>       valida um JSON de carro\n"
              << "  fsae_sim --check-track <arquivo>          valida uma pista\n"
              << "  fsae_sim --convert-track <entrada> <saida.csv> [comentario]\n"
              << "                                            converte pista (trackExcel/OptimumLap\n"
              << "                                            ou matriz do MATLAB) para o CSV do projeto\n"
              << "  fsae_sim --version\n\n"
              << "A simulacao ainda esta sendo portada (Fase 2).\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const std::string cmd = argc > 1 ? argv[1] : "";
        if (cmd == "--print-default-car") {
            std::cout << fsae::io::car_spec_to_json(fsae::CarSpec::matlab_default());
            return 0;
        }
        if (cmd == "--check-car" && argc > 2) {
            const fsae::Car car = fsae::io::load_car(argv[2]);
            std::cout << "OK: \"" << car.name << "\" (" << car.mass << " kg, " << car.gear_count()
                      << " marchas)\n";
            return 0;
        }
        if (cmd == "--check-track" && argc > 2) {
            const auto t = fsae::track::load_track(argv[2]);
            std::size_t corners = 0;
            for (const auto& s : t.segments) corners += s.is_corner() ? 1 : 0;
            std::cout << "OK: \"" << t.name << "\" (" << t.segments.size() << " segmentos, "
                      << corners << " curvas, " << t.length() << " m)\n";
            return 0;
        }
        if (cmd == "--convert-track" && argc > 3) {
            const auto t = fsae::track::load_track(argv[2]);
            fsae::track::save_track(t, argv[3], argc > 4 ? argv[4] : "");
            std::cout << "Gravado " << argv[3] << " (" << t.segments.size() << " segmentos)\n";
            return 0;
        }
        if (cmd == "--version") {
            std::cout << fsae::version() << "\n";
            return 0;
        }
        print_usage();
        return cmd.empty() || cmd == "--help" || cmd == "-h" ? 0 : 1;
    } catch (const std::exception& e) {
        std::cerr << "Erro: " << e.what() << "\n";
        return 1;
    }
}
