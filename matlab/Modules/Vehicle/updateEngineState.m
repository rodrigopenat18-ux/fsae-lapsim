function [new_motor_rpm, new_launch_mode] = updateEngineState(car, vel, current_gear, ~, ~, ~)
% updateEngineState - (V3 Modelo Piloto Perfeito) Garante uma transição suave
% da arrancada para a aceleração normal, sem queda de RPM.

    % Calcula o RPM que vem da rotação das rodas
    transmission_rpm = calculateEngineRPM(car, vel, current_gear);
    
    % LÓGICA PRINCIPAL: O RPM do motor é o maior valor entre o RPM de
    % lançamento e o RPM atual da transmissão.
    new_motor_rpm = max(car.launchRPM, transmission_rpm);
    
    % O "launch_mode" agora é implícito. Podemos considerá-lo 'false' para
    % simplificar, pois a lógica acima já resolve a transição.
    new_launch_mode = false;

end