function [drag] = calculateDrag(car, velocity) 

    drag = 0.5 * car.rho * velocity^2 * car.frontalArea * car.cd;

end