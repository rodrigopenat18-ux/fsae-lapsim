// Endurance: 25 voltas seguidas com a massa caindo pelo combustível gasto (modelo melhorado).
#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Simulation/lap.hpp"
#include "Simulation/quasi_static.hpp"
#include "Tracks/track.hpp"
#include "Vehicle/car.hpp"

using fsae::Car;
namespace sim = fsae::sim;
namespace tr = fsae::track;

namespace {

std::vector<tr::Segment> autox() {
    return tr::load_track(std::string(FSAE_DATA_DIR) + "/Tracks/autox.csv").segments;
}

sim::SimulationOptions endurance(double fuel_kg, int laps = sim::default_laps(sim::Event::Endurance)) {
    sim::SimulationOptions o;
    o.laps = laps;
    o.solver = sim::Solver::QuasiStatic;
    o.fuel_load_kg = fuel_kg;
    return o;
}

}  // namespace

TEST_CASE("endurance: prova com 25 voltas") {
    CHECK(sim::default_laps(sim::Event::Endurance) == 25);
    CHECK(sim::parse_event("endurance") == sim::Event::Endurance);
    CHECK(std::string(sim::to_string(sim::Event::Endurance)) == "endurance");
}

TEST_CASE("endurance: a massa cai com o combustivel e as voltas ficam mais rapidas") {
    const Car car = Car::matlab_default();
    const double fuel = 4.0;  // kg
    const auto r = sim::simulate(car, autox(), endurance(fuel));
    REQUIRE(r.lap_mass_kg.size() == 25);

    // Massa no meio de cada volta = carro + combustivel restante
    double used = 0;
    for (std::size_t k = 0; k < 25; ++k) {
        CAPTURE(k);
        const double lap_kg = r.lap_fuel_g[k] / 1000;
        CHECK(r.lap_mass_kg[k] == doctest::Approx(car.mass + fuel - used - 0.5 * lap_kg).epsilon(1e-4));
        if (k > 0) CHECK(r.lap_mass_kg[k] < r.lap_mass_kg[k - 1]);
        used += lap_kg;
    }
    CHECK(r.total_fuel_g / 1000 < fuel);  // o tanque nao secou

    // Mais leve a cada volta: voltas lancadas cada vez mais rapidas
    for (std::size_t k = 2; k < 25; ++k) CHECK(r.lap_times[k] < r.lap_times[k - 1]);

    // Sem o peso do combustivel tudo e mais rapido; com o tanque cheio constante, mais lento
    const auto light = sim::simulate(car, autox(), endurance(0));
    Car full = car;
    full.mass += fuel;
    const auto heavy = sim::simulate(full, autox(), endurance(0));
    for (std::size_t k = 0; k < 25; ++k) {
        CHECK(r.lap_times[k] > light.lap_times[k]);
        CHECK(r.lap_times[k] < heavy.lap_times[k]);
    }
    // Ordem de grandeza: ~1,3% de massa -> efeito de alguns centesimos por volta
    CHECK(r.lap_times[1] - light.lap_times[1] < 0.3);
    CHECK(r.lap_times[1] - light.lap_times[1] > 0.01);
}

TEST_CASE("endurance: uma correcao da massa basta") {
    const Car car = Car::matlab_default();
    const double fuel = 4.0;
    const auto o = endurance(fuel);
    const auto r = sim::simulate(car, autox(), o);
    // Mais uma iteracao com o consumo da 2ª passada
    std::vector<Car> cars(25, car);
    double used = 0;
    for (std::size_t k = 0; k < 25; ++k) {
        const double lap_kg = r.lap_fuel_g[k] / 1000;
        cars[k].mass = car.mass + fuel - used - 0.5 * lap_kg;
        used += lap_kg;
    }
    const auto again = sim::simulate_quasi_static(cars, autox(), o);
    CHECK(std::abs(again.total_time - r.total_time) < 1e-3);
    CHECK(std::abs(again.total_fuel_g - r.total_fuel_g) < 0.05);
}

TEST_CASE("endurance: temperatura do freio continua de uma volta para a outra") {
    const auto r = sim::simulate(Car::matlab_default(), autox(), endurance(4.0));
    const auto& h = r.history;
    // A volta k comeca na amostra em que a anterior termina (mesma temperatura)
    double prev_start = 0;
    for (std::size_t k = 1; k < 4; ++k) {
        const double t_start = h.brake_temp[r.lap_start_index[k]];
        CHECK(t_start > 25.0);
        CHECK(t_start >= prev_start - 1.0);
        prev_start = t_start;
    }
}

TEST_CASE("endurance: combustivel insuficiente nao deixa a massa abaixo do carro vazio") {
    const Car car = Car::matlab_default();
    const auto r = sim::simulate(car, autox(), endurance(0.5, 10));
    CHECK(r.total_fuel_g / 1000 > 0.5);
    CHECK(r.lap_mass_kg.back() == doctest::Approx(car.mass));
    for (double m : r.lap_mass_kg) CHECK(m >= car.mass);
}

TEST_CASE("modelo original: combustivel entra como massa constante") {
    const Car car = Car::matlab_default();
    sim::SimulationOptions o;
    o.laps = 2;
    o.fuel_load_kg = 4.0;
    const auto r = sim::simulate(car, autox(), o);
    CHECK(r.lap_mass_kg[0] == doctest::Approx(car.mass + 4.0));
    CHECK(r.lap_mass_kg[1] == doctest::Approx(car.mass + 4.0));
    o.fuel_load_kg = 0;
    CHECK(sim::simulate(car, autox(), o).lap_times[1] < r.lap_times[1]);
    o.fuel_load_kg = -1;
    CHECK_THROWS((void)sim::simulate(car, autox(), o));
}
