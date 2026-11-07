function [initial_gear] = findInitialGear(car, velocity_ms)
    % --- CASO ESPECIAL: Velocidade muito baixa ---
    if velocity_ms < 1.0
        initial_gear = 1;
        return;
    end
    
    % --- BUSCA PELA MELHOR MARCHA ---
    best_gear = 1;
    max_tractive_force = -1; % Inicia com um valor impossível
    
    % Loop para testar todas as marchas disponíveis
    for gear = 1:length(car.gear_ratios)
        
        % Passo 1: Calcula o RPM para a marcha atual
        rpm = calculateEngineRPM(car, velocity_ms, gear);
        
        % Passo 2: Verifica se o RPM é válido para o motor
        if rpm < car.rpm_min || rpm > car.rpm_max
            continue; % Se não for válido, pula para a próxima marcha
        end
       
        % Passo 3: Usa a função completa para obter o TORQUE NA RODA
        wheel_torque = calculateEngineTorque(car, rpm, gear);
        
        % Passo 4: Converte o torque na roda em força de tração linear
        current_tractive_force = calculateEngineForce(car, wheel_torque);
        
        % Passo 5: Compara com a melhor força encontrada até agora
        if current_tractive_force > max_tractive_force
            % Se a força desta marcha for maior, ela se torna a nossa nova melhor escolha
            max_tractive_force = current_tractive_force;
            best_gear = gear;
        end
    end
    
    initial_gear = best_gear;
end