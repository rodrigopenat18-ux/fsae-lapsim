function [final_EngineTorque] = calculateEngineTorque(car, RPM, current_gear)

    EngineTorque = car.TorqueInterpol(RPM);
    
    % Torque no final da transmissão
     
    current_gear_ratio = car.gear_ratios(current_gear); % Pega a relação da marcha atual
    
    final_EngineTorque = EngineTorque * current_gear_ratio * car.overall_reduction * car.powertrain_efficiency;
end