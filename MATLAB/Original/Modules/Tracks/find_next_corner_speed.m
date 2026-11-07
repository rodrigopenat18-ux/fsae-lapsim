function next_speed = find_next_corner_speed(current_idx, num_segments, segments, corner_speeds)
% Procura à frente no traçado qual a velocidade da próxima curva.
    for j = current_idx + 1 : num_segments
        if segments(j, 3) > 0 % Se o raio for > 0, é uma curva
            next_speed = corner_speeds(j);
            return;
        end
    end
    % Se não houver mais curvas (ex: reta da chegada), o alvo é velocidade máxima
    % para que o carro acelere até o fim.
    next_speed = inf; 
end