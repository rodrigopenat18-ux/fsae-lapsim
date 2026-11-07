function v_max = calculateMaxSafeCurveSpeed(car, R, slope, segmentIdx)
    % R: Raio da curva (m)
    % slope: Inclinação (rad)
    % segmentIdx: Índice da curva para o print
    
    % --- 1. Limite por Aderência (Grip) ---
    % Cálculo analítico considerando o Downforce que aumenta com a velocidade
    K_aero = 0.5 * car.rho * car.cl * car.frontalArea;
    FN_peso = car.mass * car.g * cos(slope);
    
    % Fórmula baseada em F_centripeta = F_atrito (incluindo aero)
    v_sq_grip = (car.muy * FN_peso * R) / (car.mass - car.muy * K_aero * R);
    v_grip = sqrt(max(0, v_sq_grip));
    
    % --- 2. Limite por Tombamento (Carga Zero / Roda Levantando) ---
    % Testamos de 1 em 1 km/h para ver quando a carga normal zera
    v_tip = v_grip; 
    step = 0.5; % Passo de 0.5 m/s para maior velocidade de cálculo
    
    for v_test = 1:step:v_grip
        % Aceleração lateral em G
        accLat_g = (v_test^2 / R) / car.g;
        
        % --- CORREÇÃO: Passar a velocidade (v_test) para a função ---
        % Agora a função considera o peso + transferência lateral + DOWNFORCE
        [Normal_N, ~] = calculateLoadTransfer(car, 0, accLat_g, slope, v_test); 
        
        % Se qualquer roda tiver carga <= 0 (carro começou a tombar ou empinar)
        if any(Normal_N <= 0.01) % Usamos 0.01 para evitar erros de precisão
            v_tip = v_test - step; 
            break;
        end
    end
    
    % --- 3. Resultado Final ---
    v_max = min(v_grip, v_tip);
    
    % Log apenas se o limite for o tombamento e não o grip
    if v_tip < (v_grip - 0.2)
        fprintf(['\n[AVISO] CURVA %d: Roda levantou (Carga <= 0)!\n' ...
                 '      Velocidade de Grip:        %.2f km/h\n' ...
                 '      Velocidade por Tombamento: %.2f km/h (LIMITANTE)\n'], ...
                 segmentIdx, v_grip * 3.6, v_tip * 3.6);
    end
end