function [downforce] = calculateDownforce(car, velocity) 

    downforce = 0.5 * car.rho * velocity^2 * car.frontalArea * car.cl;

end