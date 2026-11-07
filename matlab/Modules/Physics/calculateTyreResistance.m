function [RR] = calculateTyreResistance(car, velocity, slope, downforce)

    % Coeficiente f 
    ff = 0.01 * (1 + velocity * 3.6 / 120);
    
    % Força resistiva do pneu
    RR = ff * (car.mass * car.g * cos(slope) + downforce); 

end