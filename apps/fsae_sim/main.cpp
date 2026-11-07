#include <iostream>

#include "fsae/version.hpp"

int main() {
    std::cout << "fsae_sim " << fsae::version() << "\n"
              << "Simulador de volta FSAE (port C++ do simulador MATLAB).\n"
              << "Fase 0: esqueleto do projeto. A simulacao ainda nao foi implementada.\n";
    return 0;
}
