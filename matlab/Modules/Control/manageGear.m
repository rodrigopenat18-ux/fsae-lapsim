function [new_gear, shift_status] = manageGear(car, velocity, current_gear, isAccelerating, shift_status, dt)
% Gerencia o estado da caixa de marchas, decidindo por upshifts, downshifts
% e controlando o tempo da troca.

    new_gear = current_gear; % Inicia com a marcha atual

    % --- Passo 1: Verifica se uma troca já está em andamento ---
    if shift_status.isShifting
        shift_status.timer = shift_status.timer - dt; % Decrementa o tempo
        
        % Se o tempo de troca acabou, finaliza o processo
        if shift_status.timer <= 0
            shift_status.isShifting = false;
            shift_status.timer = 0;
        end
        % Durante a troca, a marcha não muda e nenhuma nova troca pode começar
        return;
    end

    % --- Passo 2: Se não há troca em andamento, verifica se uma nova é necessária ---
    potential_new_gear = current_gear;
    if isAccelerating
        % Se está acelerando, checa por upshift
        potential_new_gear = checkUpshift(car, velocity, current_gear, shift_status.isShifting);
    else
        % Se está freando, checa por downshift
        potential_new_gear = checkDownshift(car, velocity, current_gear, shift_status.isShifting);
    end

    % --- Passo 3: Se uma troca foi decidida, inicia o processo ---
    if potential_new_gear ~= current_gear
        shift_status.isShifting = true;
        shift_status.timer = car.shiftTime; % Define o tempo necessário para a troca
        new_gear = potential_new_gear; % Atualiza a marcha
    end
end