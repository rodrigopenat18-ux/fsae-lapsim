function [new_gear] = checkDownshift(car, velocity, current_gear, isShifting)
    % Verifica se uma redução de marcha é necessária.
    
    new_gear = current_gear; % Inicia com a marcha atual.
    
    % Se já estiver na primeira marcha ou trocando, não faz nada.
    if current_gear <= 1 || isShifting
        return;
    end
    
    % Calcula o RPM atual para a verificação.
    rpm_atual = calculateEngineRPM(car, velocity, current_gear);
    
    % CONDIÇÃO CORRIGIDA:
    % Compara o RPM atual com o ponto ÚNICO de downshift.
    if rpm_atual < car.downshiftRPM
        % Se o RPM caiu abaixo do limite, desce uma marcha.
        new_gear = current_gear - 1;
    end
end