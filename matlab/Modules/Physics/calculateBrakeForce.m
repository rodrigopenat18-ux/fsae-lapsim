function [brake_Force] = calculateBrakeForce(car)

    brake_Force = 3000 / car.tire_radius;

end