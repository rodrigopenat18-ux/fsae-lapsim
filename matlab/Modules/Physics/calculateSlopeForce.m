function [slopeForce] = calculateSlopeForce(car, slope)
    
    slopeForce = (car.mass * car.g) * sin(slope);
    
end