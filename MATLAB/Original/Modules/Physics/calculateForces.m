function [engineForce, drag, rollingResistance, slopeForce, new_gear, new_shift_status, downforce] = ...
    calculateForces(car, velocity, slope, current_gear, isAccelerating, shift_status, dt, current_time) % Adicionado 'current_time'

    % 1. Forças Resistivas 
    downforce = calculateDownforce(car, velocity);

    % --- %
    drag = calculateDrag(car, velocity);
    rollingResistance = calculateTyreResistance(car, velocity, slope, downforce);
    slopeForce = calculateSlopeForce(car, slope);
    
    % 2. Gerenciamento de Marcha 
    [new_gear, new_shift_status] = manageGear(car, velocity, current_gear, isAccelerating, shift_status, dt);
    
    % 3. Cálculo da Força do Motor
    engineForce = 0;
    
    if ~new_shift_status.isShifting
        
        if velocity < (2*pi/60) * car.launchRPM * car.tire_radius / (car.gear_ratios(current_gear) * car.overall_reduction) % Lógica de arrancada
            engine_torque = calculateLaunchTorque(car);
            gear_ratio = car.gear_ratios(new_gear);
            wheel_torque = engine_torque * gear_ratio * car.overall_reduction * car.powertrain_efficiency;
       
            
        else % Lógica normal
            rpm = calculateEngineRPM(car, velocity, new_gear);
            wheel_torque = calculateEngineTorque(car, rpm, new_gear);
        end
        engineForce = calculateEngineForce(car, wheel_torque);
    end
end