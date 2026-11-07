function [EngineRPM] = calculateEngineRPM(car, velocity, current_gear)

    total_gear_ratio = car.gear_ratios(current_gear) * car.overall_reduction;
    EngineRPM = (velocity / car.tire_radius) * total_gear_ratio * (60 / (2*pi));

end