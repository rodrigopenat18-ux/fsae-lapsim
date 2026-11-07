#include "fsae/physics/load_transfer.hpp"

#include <cmath>

#include "fsae/physics/resistances.hpp"

namespace fsae::physics {

WheelLoads static_loads(const Car& car, double slope) noexcept {
    const double perpendicular_weight = car.mass * car.g * std::cos(slope);
    const double front = (1 - car.weight_dist_rear_percent / 100) * perpendicular_weight / 2;
    const double rear = (car.weight_dist_rear_percent / 100) * perpendicular_weight / 2;
    return {front, front, rear, rear};
}

WheelLoads dynamic_transfer(const Car& car, double ax_g, double ay_g) noexcept {
    const double ax = ax_g * car.g;
    const double ay = ay_g * car.g;
    const double delta_long = (car.cg_height / car.wheelbase) * car.mass * ax / 2;
    const double delta_lat_front = (car.cg_height / car.track_front) * car.mass * ay / 2;
    const double delta_lat_rear = (car.cg_height / car.track_rear) * car.mass * ay / 2;
    return {
        -delta_long + delta_lat_front,  // DE
        -delta_long - delta_lat_front,  // DD
        +delta_long - delta_lat_rear,   // TD
        +delta_long + delta_lat_rear,   // TE
    };
}

LoadBase load_base(const Car& car, double slope, double velocity) noexcept {
    LoadBase b;
    b.static_n = static_loads(car, slope);
    const double f_down = downforce(car, velocity);
    const double dist_front = (1 - car.cp_aero / 100);
    const double dist_rear = (car.cp_aero / 100);
    const double half_down = f_down / 2;
    b.aero_n = {dist_front * half_down, dist_front * half_down, dist_rear * half_down,
                dist_rear * half_down};
    return b;
}

WheelLoads normal_loads(const Car& car, const LoadBase& base, double ax_g, double ay_g) noexcept {
    const WheelLoads dyn = dynamic_transfer(car, ax_g, ay_g);
    WheelLoads n;
    for (std::size_t i = 0; i < 4; ++i) n[i] = base.static_n[i] + dyn[i] + base.aero_n[i];
    return n;
}

LoadTransferResult load_transfer(const Car& car, double ax_g, double ay_g, double slope,
                                 double velocity) noexcept {
    LoadTransferResult r;
    r.normal_n = normal_loads(car, load_base(car, slope, velocity), ax_g, ay_g);
    const double total = r.normal_n[0] + r.normal_n[1] + r.normal_n[2] + r.normal_n[3];
    for (std::size_t i = 0; i < 4; ++i) r.distribution[i] = (r.normal_n[i] / total) * 100;
    return r;
}

}  // namespace fsae::physics
