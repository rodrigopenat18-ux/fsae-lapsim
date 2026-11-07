function [launch_torque] = calculateLaunchTorque(car)
% Simula um "two-step" 
% Pega o torque real do motor na rotação de arrancada pré-definida.

    % Acessa o RPM de arrancada definido no carro 
    launch_rpm = car.launchRPM;
    
    % Usa o objeto de interpolação para encontrar o torque do motor
    % correspondente àquela rotação.
    launch_torque = car.TorqueInterpol(launch_rpm);
    
end