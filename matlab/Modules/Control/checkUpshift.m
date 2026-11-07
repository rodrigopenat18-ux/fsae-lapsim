function [new_gear] = checkUpshift(car, velocity, current_gear, is_shifting)
% checkUpshift - Verifica se um upshift é necessário com base em um RPM
% específico para a marcha atual.

    new_gear = current_gear; % Por padrão, não há troca
    
    % Não faz nada se já estiver trocando ou na última marcha
    if is_shifting || current_gear >= length(car.gear_ratios)
        return;
    end
    
    % Calcula o RPM atual
    current_rpm = calculateEngineRPM(car, velocity, current_gear);
    
    % --- LÓGICA PRINCIPAL ---
    % Pega o RPM de troca específico para a marcha ATUAL do nosso vetor
    rpm_para_troca = car.shiftRPM_vec(current_gear);
    
    % Se o RPM atual for maior que o RPM de troca para esta marcha, sobe a marcha
    if current_rpm > rpm_para_troca
        new_gear = current_gear + 1;
    end
end