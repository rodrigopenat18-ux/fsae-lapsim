function runSimulation(modo_otimizacao, robot_trackType, robot_trackName)
%% --------------------- runSimulation.m (FINAL - SI + TÉRMICO + FUEL + LOAD) ---------------------
% Descrição: Executa a simulação FSAE. 
%            Histórico salvo em Unidades SI (m/s^2).
%            INCLUI MODELO TÉRMICO (Radiação + Carga Dinâmica)
%            INCLUI MODELO DE CONSUMO (BSFC)

    % 1. Configuração do Modo de Operação
    if nargin < 1
        modo_otimizacao = false; % Padrão: Modo Humano (Visual)
    end
    
    if ~modo_otimizacao
        clc; close all; 
    end
    
    % Inicializa os caminhos das pastas
    setup_paths;

    %% --- 1. Seleção da Pista ---
    if modo_otimizacao
        % --- LÓGICA DO ROBÔ ---
        if nargin >= 2
            trackType = robot_trackType;
        else
            trackType = 1; % Default se não passar o argumento
        end
        
        if trackType == 4
            if nargin >= 3
                fileName = robot_trackName;
            else
                fileName = "track_AutoX_2024_V1"; 
            end
        end
    else
        % --- LÓGICA DO HUMANO ---
        trackType = input("Qual será a pista:\n 1 - Autocross\n 2 - Skidpad\n 3 - Aceleração\n 4 - Pista Customizada\n");
    end

    % Carregament dos Segmentos
    if trackType == 1 
        segments = trackAutoX();
    elseif trackType == 2
        segments = trackSkidPad();
    elseif trackType == 3 
        segments = trackAcceleration();
    elseif trackType == 4
        if ~modo_otimizacao
            fileName = input("Digite o nome da função do circuito (ex: trackCustom): \n", 's');
        end
        
        try
            if isstring(fileName) || ischar(fileName)
                trackFunction = str2func(fileName);
            else
                trackFunction = fileName; 
            end
            segments = trackFunction();
        catch ME
            fprintf('Erro ao carregar função da pista: "%s".\n', fileName);
            return; 
        end
    else 
        fprintf("Opção inválida.\n"); return; 
    end

    % Gera visualização inicial
    if ~modo_otimizacao
        fprintf('Gerando prévia da pista...\n');
    end
    
    [X, Y, Z, ~] = buildTrack(segments); 
    
    if modo_otimizacao
        close(gcf); 
    end

    %% --- 2. Configuração Interativa de Partida ---
    initial_velocity = 0; % FIXO EM 0
    
    if modo_otimizacao
        escolha = 'Não (Padrão)';
    else
        if trackType == 1 || trackType == 4
            escolha = questdlg('Deseja escolher onde a volta começa?', ...
                'Configuração de Partida', 'Sim', 'Não (Padrão)', 'Sim');
        else
            escolha = 'Não (Padrão)'; 
        end
    end

    if strcmp(escolha, 'Sim')
        figure('Name', 'SELECIONE O PONTO DE PARTIDA', 'NumberTitle', 'off');
        plot(X, Y, 'k-', 'LineWidth', 2);
        axis equal; grid on;
        title('CLIQUE no local de início (Linha de Largada)');
        
        [xk, yk] = ginput(1); 
        close(gcf); 
        
        pontos_por_segmento = 150; 
        distancias = sqrt((X - xk).^2 + (Y - yk).^2);
        [~, idx_min] = min(distancias);
        segmento_escolhido = ceil(idx_min / pontos_por_segmento);
        segmento_escolhido = min(max(segmento_escolhido, 1), size(segments, 1));
        
        fprintf('\n>>> Ponto de partida definido no segmento #%d.\n', segmento_escolhido);
        segments = circshift(segments, -(segmento_escolhido - 1), 1);
        [X, Y, Z, ~] = buildTrack(segments);
    end
    
    track_geom = struct('X', X, 'Y', Y, 'Z', Z);

    %% --- 3. Preparação ---
    if ~modo_otimizacao
        fprintf('\nInicializando veículo e física...\n');
    end
    
    carro = initializeCar();
    [vel_curvas_base, slope_base, ~] = initializeTrack(carro, segments);
    
    % --- Lógica de 2 Voltas ---
    is_multi_lap = (trackType == 1 || trackType == 4);
    
    if is_multi_lap
        segments_total = [segments; segments];
        vel_curvas_total = [vel_curvas_base; vel_curvas_base];
        slope_total = [slope_base; slope_base];
        if ~modo_otimizacao
            fprintf('>> Modo Circuito: Simulando 2 Voltas (Lançamento + Volta Rápida).\n');
        end
    else
        segments_total = segments;
        vel_curvas_total = vel_curvas_base;
        slope_total = slope_base;
        if ~modo_otimizacao
            fprintf('>> Modo Tiro Único: Simulando apenas 1 passagem.\n');
        end
    end

    %% --- 4. Simulação Principal ---
    run_plot_flag = ~modo_otimizacao;
    run_verbose   = ~modo_otimizacao;
    
    % *** Chama a função de loop atualizada ***
    [time_Total, history_With_Slope] = runSimulationLoop(...
        trackType, segments_total, carro, vel_curvas_total, slope_total, ...
        run_plot_flag, track_geom, initial_velocity, size(segments, 1), is_multi_lap, run_verbose); 

    %% --- 5. Pós-Processamento e Saída ---
    
    SimOut.History = history_With_Slope;
    SimOut.Track   = track_geom; 
    SimOut.Car     = carro;       
    
    SimOut.Results.TotalTime   = time_Total;
    SimOut.Results.MaxVelocity = max(history_With_Slope.velocity) * 3.6; % km/h
    SimOut.Results.MaxLatG     = max(abs(history_With_Slope.lateral_accel)) / 9.81;
    
    % Temperatura Máxima
    if isfield(history_With_Slope, 'brake_temp')
        SimOut.Results.MaxBrakeTemp = max(history_With_Slope.brake_temp);
    else
        SimOut.Results.MaxBrakeTemp = 0;
    end
    
    % Combustível Total
    SimOut.Results.TotalFuel_g = history_With_Slope.fuel_consumed_g(end);
    
    % --- Separação dos Tempos e Consumo de Volta ---
    if is_multi_lap
        distancia_total_pista = sum(segments(:,2));
        idx_end_lap1 = find(history_With_Slope.distance >= distancia_total_pista, 1);
        
        if ~isempty(idx_end_lap1)
            % Tempos
            time_lap1 = history_With_Slope.time(idx_end_lap1);
            time_lap2 = time_Total - time_lap1;
            SimOut.Results.Lap1_Time = time_lap1;
            SimOut.Results.Lap2_Time = time_lap2;
            SimOut.Results.FastestLap = time_lap2; 
            
            % Combustível
            fuel_lap1 = history_With_Slope.fuel_consumed_g(idx_end_lap1);
            fuel_lap2 = SimOut.Results.TotalFuel_g - fuel_lap1;
        else
            % Fallback caso a volta não feche
            SimOut.Results.Lap1_Time = time_Total;
            SimOut.Results.Lap2_Time = NaN;
            SimOut.Results.FastestLap = time_Total;
            fuel_lap1 = SimOut.Results.TotalFuel_g;
            fuel_lap2 = 0;
        end
    else
        SimOut.Results.Lap1_Time = time_Total;
        SimOut.Results.Lap2_Time = 0;
        SimOut.Results.FastestLap = time_Total;
        fuel_lap1 = SimOut.Results.TotalFuel_g;
    end

    if ~modo_otimizacao
        fprintf('\n--- Relatório de Consumo ---\n');
        if is_multi_lap
            fprintf('Consumo Volta 1:   %.2f g\n', fuel_lap1);
            fprintf('Consumo Volta 2:   %.2f g\n', fuel_lap2);
        end
        fprintf('Consumo Total:     %.2f g\n', SimOut.Results.TotalFuel_g);
        dist_km = history_With_Slope.distance(end) / 1000;
        if dist_km > 0
            fprintf('Eficiência:        %.2f g/km\n', SimOut.Results.TotalFuel_g / dist_km);
        end
    end

    %% --- 6. Decisão Final ---
    if modo_otimizacao
        save('Optimization_Result.mat', 'SimOut');
    else
        fprintf('\n----------------------------------------\n');
        if is_multi_lap
            fprintf('Volta 1 (Partida): %.3f s\n', SimOut.Results.Lap1_Time);
            fprintf('Volta 2 (Lançada): %.3f s\n', SimOut.Results.Lap2_Time);
        end
        fprintf('Tempo Total:       %.3f s\n', SimOut.Results.TotalTime);
        fprintf('Temp. Máx Freio:   %.1f °C\n', SimOut.Results.MaxBrakeTemp);
        fprintf('----------------------------------------\n');
        
        choice = input(['\nDeseja comparar os resultados?\n' ...
                        ' 1 - Não\n' ...
                        ' 2 - Sim (vs. Log Real)\n' ...
                        ' 3 - Sim (vs. OPLap Log)\n' ...
                        ' 4 - Sim (vs. Simulação Plana [Sem Slope])\n' ...
                        'Escolha: ']);
                        
        if choice == 2
            [real_log_history, success] = loadRealLog(trackType);
            if success, plotComparison(history_With_Slope, real_log_history, trackType, 'Log Real'); end
        elseif choice == 3
             if trackType == 1, oplap_file = 'Pista_AutoX_OPLap.csv';
            elseif trackType == 3, oplap_file = 'Pista_Accel_OPLap.csv';
            else, oplap_file = ''; end
            if ~isempty(oplap_file)
                [oplap_history, success_op] = loadOPLapLog(oplap_file);
                if success_op, plotComparison(history_With_Slope, oplap_history, trackType, 'OPLap Log'); end
            end
        elseif choice == 4
            if trackType == 2
                fprintf('Skidpad já é plano.\n');
            else
                slope_flat = zeros(size(slope_total));
                [time_Flat, history_Flat] = runSimulationLoop(...
                    trackType, segments_total, carro, vel_curvas_total, slope_flat, ...
                    false, track_geom, initial_velocity, size(segments, 1), is_multi_lap, false); 
                fprintf('Tempo Total (Com Slope): %.3fs | Tempo Total (Plana): %.3fs\n', time_Total, time_Flat);
                plotComparison(history_With_Slope, history_Flat, trackType, 'Simulação Plana');
            end
        end
        fprintf('\nFim da execução.\n');
    end
end

%% =========================================================================
%                     FUNÇÃO DE SIMULAÇÃO (Lógica Física)
% =========================================================================
function [total_time, full_history] = runSimulationLoop(trackType, segments, carro, ...
                                                        velocidades_curvas, slopes, ...
                                                        plot_flag, track_geom, initial_vel, ...
                                                        num_seg_per_lap, is_multi_lap, verbose)
    
    if ~exist('initial_vel', 'var'), initial_vel = 0; end
    if ~exist('verbose', 'var'), verbose = true; end
    
    if verbose
        fprintf('\n=======================================================\n');
        fprintf(' INICIANDO SIMULAÇÃO: %d TRECHOS TOTAIS\n', size(segments,1));
        fprintf(' Velocidade de Partida: %.1f km/h\n', initial_vel*3.6);
        fprintf('=======================================================\n');
    end
    
    total_time = 0;
    current_vel = initial_vel;
    
    % --- INICIALIZAÇÃO TÉRMICA ---
    current_temp = 25; % Começa com temperatura ambiente
    
    distancia_volta_1 = 0;
    if is_multi_lap
        for k = 1:num_seg_per_lap, distancia_volta_1 = distancia_volta_1 + segments(k, 2); end
    end
    
    % Inicializa histórico (Adicionado brake_temp, h_coef, fuel_consumed_g)
    % E AGORA: P_conv, P_rad, P_cond, load_front_dist
    full_history = struct('time', 0, 'distance', 0, 'velocity', current_vel, ...
                          'gear', 1, 'acceleration', 0, 'rpm', carro.rpm_min, ...
                          'lateral_accel', 0, ...
                          'drag', 0, 'downforce', 0, 'rolling_resistance', 0, ...
                          'slope_force', 0, 'engine_force', 0, 'brake_force', 0, ...
                          'brake_temp', current_temp, 'h_coef', 0, 'fuel_consumed_g', 0, ...
                          'P_conv', 0, 'P_rad', 0, 'P_cond', 0, ...               % <--- NOVO
                          'load_front_dist', (1-carro.weight_dist_rear_percent/100)*100); % <--- NOVO
                          
    if current_vel > 1
        full_history.gear(1) = findInitialGear(carro, current_vel);
        full_history.rpm(1) = calculateEngineRPM(carro, current_vel, full_history.gear(1));
    end
    
    num_total_segments = size(segments, 1);
    skip_steps = 0;
    total_fuel_g = 0; % Inicializa o acumulador de combustível global
    
    for i = 1:num_total_segments
        if skip_steps > 0
            skip_steps = skip_steps - 1;
            continue;
        end
        
        if is_multi_lap
            volta_atual = ceil(i / num_seg_per_lap);
            seg_na_volta = mod(i-1, num_seg_per_lap) + 1;
        else
            volta_atual = 1; seg_na_volta = i;
        end
        
        L = segments(i, 2);
        R = segments(i, 3);
        slope = slopes(i);
        type = segments(i, 5);
        
        % --- CURVA ---
        if R ~= 0 
            max_v_fisica = velocidades_curvas(i);
            % A velocidade real é o que o carro traz, mas limitada pelo grip
            % Se current_vel > max_v_fisica, significa que o freio falhou em atingir o alvo
            actual_v = min(current_vel, max_v_fisica);
            
            if verbose
                fprintf('\n[VOLTA %d] TRECHO %02d (CURVA) | R: %.1fm | L: %.1fm\n', volta_atual, seg_na_volta, R, L);
                fprintf('      > Alvo de Segurança: %.2f km/h\n', max_v_fisica * 3.6);
                fprintf('      > Entrada Real:      %.2f km/h\n', current_vel * 3.6);
                
                if current_vel > (max_v_fisica + 0.1) 
                    fprintf('      [!] ALERTA: O carro não freou o suficiente! Entrou acima do limite de tombamento.\n');
                end
            end
            
            time_seg = L / max(actual_v, 0.1);
            
            if time_seg > 0
                nP = max(2, floor(time_seg / 0.05));
                t_add = linspace(0, time_seg, nP+1);
                d_add = linspace(0, L, nP+1);
                
                base_t = full_history.time(end); 
                base_d = full_history.distance(end);
                
                gear = findInitialGear(carro, actual_v); 
                rpm = calculateEngineRPM(carro, actual_v, gear);
                lat_accel_ms2 = (actual_v^2 / R) * ((-1)^(type==1)); 
                lat_accel_g = lat_accel_ms2 / 9.81; % Conversão para G
                
                F_drag = calculateDrag(carro, actual_v);
                F_down = calculateDownforce(carro, actual_v);
                F_slope = calculateSlopeForce(carro, slope);
                F_roll = calculateTyreResistance(carro, actual_v, slope, F_down);
                F_engine = F_drag + F_roll + F_slope; 
                
                % --- CÁLCULO TÉRMICO E DE CONSUMO NA CURVA ---
                temp_vec = zeros(1, nP);
                h_vec = zeros(1, nP); 
                fuel_vec = zeros(1, nP);
                
                % Vetores para os novos dados térmicos e de carga
                p_conv_vec = zeros(1, nP); 
                p_rad_vec  = zeros(1, nP); 
                p_cond_vec = zeros(1, nP); 
                load_vec   = zeros(1, nP);
                
                dt_curve = time_seg / nP;
                
                % Cálculo da Distribuição de Carga Lateral na Curva
                % (Assume aceleração longitudinal ~ 0, foco no lateral)
                [Normal_N, ~] = calculateLoadTransfer(carro, 0, lat_accel_g, slope, actual_v);
                front_dist_val = (Normal_N(1) + Normal_N(2)) / sum(Normal_N) * 100;
                ratio_val = front_dist_val / 100;

                for j=1:nP
                    % TÉRMICO: Chama função atualizada com 6 argumentos de SAÍDA e ENTRADA
                    [current_temp, ~, pc, pr, pcond, h_val] = calculateBrakeTemp(carro, actual_v, 0, current_temp, dt_curve, ratio_val);
                    
                    temp_vec(j)   = current_temp;
                    h_vec(j)      = h_val;
                    p_conv_vec(j) = pc;
                    p_rad_vec(j)  = pr;
                    p_cond_vec(j) = pcond;
                    load_vec(j)   = front_dist_val;
                
                    % CONSUMO:
                    pot_kW = max(0, (F_engine * actual_v) / 1000); % Proteção para não gerar potência negativa
                    bsfc_inst = carro.bsfc_map(rpm);
                    fuel_step = (bsfc_inst * pot_kW * dt_curve) / 3600;
                    total_fuel_g = total_fuel_g + fuel_step;
                    fuel_vec(j) = total_fuel_g;
                end
                
                % Update History
                full_history.time = [full_history.time, base_t+t_add(2:end)];
                full_history.distance = [full_history.distance, base_d+d_add(2:end)];
                full_history.velocity = [full_history.velocity, repmat(actual_v,1,nP)];
                full_history.gear = [full_history.gear, repmat(gear,1,nP)];
                full_history.acceleration = [full_history.acceleration, zeros(1,nP)];
                full_history.rpm = [full_history.rpm, repmat(rpm,1,nP)];
                full_history.lateral_accel = [full_history.lateral_accel, repmat(lat_accel_ms2,1,nP)];
                
                full_history.drag = [full_history.drag, repmat(F_drag, 1, nP)];
                full_history.downforce = [full_history.downforce, repmat(F_down, 1, nP)];
                full_history.rolling_resistance = [full_history.rolling_resistance, repmat(F_roll, 1, nP)];
                full_history.slope_force = [full_history.slope_force, repmat(F_slope, 1, nP)];
                full_history.engine_force = [full_history.engine_force, repmat(F_engine, 1, nP)];
                full_history.brake_force = [full_history.brake_force, zeros(1, nP)];
                
                % Salva os dados estendidos
                full_history.brake_temp = [full_history.brake_temp, temp_vec];
                full_history.h_coef = [full_history.h_coef, h_vec]; 
                full_history.fuel_consumed_g = [full_history.fuel_consumed_g, fuel_vec];
                
                full_history.P_conv = [full_history.P_conv, p_conv_vec];
                full_history.P_rad = [full_history.P_rad, p_rad_vec];
                full_history.P_cond = [full_history.P_cond, p_cond_vec];
                full_history.load_front_dist = [full_history.load_front_dist, load_vec];
            end
            current_vel = actual_v;
            total_time = total_time + time_seg;
            
        % --- RETA ---
        else 
            temp_L = L;
            temp_Slope_accum = slope * L;
            count_merge = 0;
            for k = (i+1):num_total_segments
                if segments(k, 3) == 0 % É reta
                    count_merge = count_merge + 1;
                    temp_L = temp_L + segments(k, 2);
                    temp_Slope_accum = temp_Slope_accum + (slopes(k) * segments(k, 2));
                else
                    break; 
                end
            end
            
            if count_merge > 0
                final_slope = temp_Slope_accum / temp_L;
                if verbose
                    fprintf('\n[VOLTA %d] TRECHO %02d (RETA FUNDIDA: %d segs) | Total: %.1fm\n', ...
                            volta_atual, seg_na_volta, count_merge+1, temp_L);
                    fprintf('      > Entrada: %.1f km/h\n', current_vel*3.6);
                end
                idx_next_curve = i + count_merge; 
                target_v = find_next_corner_speed(idx_next_curve, num_total_segments, segments, velocidades_curvas);
                
                % *** calculate_Straight já retorna Fuel, Thermal e LOAD atualizados ***
                [time_seg_total, h_seg] = calculate_Straight(carro, temp_L, current_vel, target_v, final_slope, trackType, current_temp);
                
                if time_seg_total == -1, return; end
                
                if length(h_seg.time) > 1
                   full_history.time = [full_history.time, h_seg.time(2:end) + full_history.time(end)];
                   full_history.distance = [full_history.distance, h_seg.position(2:end) + full_history.distance(end)];
                   full_history.velocity = [full_history.velocity, h_seg.velocity(2:end)];
                   full_history.gear = [full_history.gear, h_seg.gear(2:end)];
                   full_history.acceleration = [full_history.acceleration, h_seg.acceleration(2:end)];
                   full_history.rpm = [full_history.rpm, h_seg.rpm(2:end)];
                   full_history.lateral_accel = [full_history.lateral_accel, zeros(1, length(h_seg.time)-1)];
                   
                   full_history.drag = [full_history.drag, h_seg.drag(2:end)];
                   full_history.downforce = [full_history.downforce, h_seg.downforce(2:end)];
                   full_history.rolling_resistance = [full_history.rolling_resistance, h_seg.rolling_resistance(2:end)];
                   full_history.slope_force = [full_history.slope_force, h_seg.slope_force(2:end)];
                   full_history.engine_force = [full_history.engine_force, h_seg.engine_force(2:end)];
                   full_history.brake_force = [full_history.brake_force, h_seg.brake_force(2:end)];
                   
                   % Salva Temperatura, H e Consumo Acumulado
                   full_history.brake_temp = [full_history.brake_temp, h_seg.brake_temp(2:end)];
                   full_history.h_coef = [full_history.h_coef, h_seg.h_coef(2:end)]; 
                   full_history.fuel_consumed_g = [full_history.fuel_consumed_g, h_seg.fuel_consumed_g(2:end) + total_fuel_g];
                   
                   % Salva os NOVOS CAMPOS vindos da reta
                   full_history.P_conv = [full_history.P_conv, h_seg.P_conv(2:end)];
                   full_history.P_rad  = [full_history.P_rad, h_seg.P_rad(2:end)];
                   full_history.P_cond = [full_history.P_cond, h_seg.P_cond(2:end)];
                   full_history.load_front_dist = [full_history.load_front_dist, h_seg.load_front_dist(2:end)];
                   
                   total_fuel_g = full_history.fuel_consumed_g(end);
                   current_vel = h_seg.velocity(end);
                   current_temp = h_seg.brake_temp(end); 
                end
                
                if verbose
                     fprintf('      > Alvo Final: %.1f km/h\n', target_v*3.6);
                     fprintf('      > SAÍDA COMBINADA: %.1f km/h | Tempo Total: %.3fs\n', current_vel*3.6, time_seg_total);
                end
                total_time = total_time + time_seg_total;
                skip_steps = count_merge;
            else
                % Reta Simples
                if verbose
                    fprintf('\n[VOLTA %d] TRECHO %02d (RETA) | Comprimento: %.1fm\n', volta_atual, seg_na_volta, L);
                    fprintf('      > Entrada: %.1f km/h\n', current_vel*3.6);
                end
                target_v = find_next_corner_speed(i, num_total_segments, segments, velocidades_curvas);
                
                [time_seg, h_seg] = calculate_Straight(carro, L, current_vel, target_v, slope, trackType, current_temp);
                if time_seg == -1, return; end
                
                if length(h_seg.time) > 1
                   full_history.time = [full_history.time, h_seg.time(2:end) + full_history.time(end)];
                   full_history.distance = [full_history.distance, h_seg.position(2:end) + full_history.distance(end)];
                   full_history.velocity = [full_history.velocity, h_seg.velocity(2:end)];
                   full_history.gear = [full_history.gear, h_seg.gear(2:end)];
                   full_history.acceleration = [full_history.acceleration, h_seg.acceleration(2:end)];
                   full_history.rpm = [full_history.rpm, h_seg.rpm(2:end)];
                   full_history.lateral_accel = [full_history.lateral_accel, zeros(1, length(h_seg.time)-1)];
                   
                   full_history.drag = [full_history.drag, h_seg.drag(2:end)];
                   full_history.downforce = [full_history.downforce, h_seg.downforce(2:end)];
                   full_history.rolling_resistance = [full_history.rolling_resistance, h_seg.rolling_resistance(2:end)];
                   full_history.slope_force = [full_history.slope_force, h_seg.slope_force(2:end)];
                   full_history.engine_force = [full_history.engine_force, h_seg.engine_force(2:end)];
                   full_history.brake_force = [full_history.brake_force, h_seg.brake_force(2:end)];
                   
                   full_history.brake_temp = [full_history.brake_temp, h_seg.brake_temp(2:end)];
                   full_history.h_coef = [full_history.h_coef, h_seg.h_coef(2:end)]; 
                   full_history.fuel_consumed_g = [full_history.fuel_consumed_g, h_seg.fuel_consumed_g(2:end) + total_fuel_g];
                   
                   % Salva os NOVOS CAMPOS vindos da reta
                   full_history.P_conv = [full_history.P_conv, h_seg.P_conv(2:end)];
                   full_history.P_rad  = [full_history.P_rad, h_seg.P_rad(2:end)];
                   full_history.P_cond = [full_history.P_cond, h_seg.P_cond(2:end)];
                   full_history.load_front_dist = [full_history.load_front_dist, h_seg.load_front_dist(2:end)];

                   total_fuel_g = full_history.fuel_consumed_g(end);
                   current_vel = h_seg.velocity(end);
                   current_temp = h_seg.brake_temp(end); 
                end
                
                if verbose, fprintf('      > SAÍDA: %.1f km/h | Tempo: %.3fs\n', current_vel*3.6, time_seg); end
                total_time = total_time + time_seg;
            end
        end
    end
    
    % --- PLOTAGEM ---
    if plot_flag
        if is_multi_lap
            if verbose, fprintf('\n[Plot] Gerando gráficos APENAS da Volta 2...\n'); end
            idx_lap2 = full_history.distance >= distancia_volta_1;
            if sum(idx_lap2) < 2
                if verbose, fprintf('[Aviso] Erro no recorte da volta. Plotando tudo.\n'); end
                plotResults(track_geom, full_history, carro);
            else
                lap2_history = struct();
                start_t = full_history.time(find(idx_lap2, 1));
                
                % Copia campos básicos
                fields = fieldnames(full_history);
                for k = 1:length(fields)
                    fName = fields{k};
                    if isvector(full_history.(fName)) && length(full_history.(fName)) == length(idx_lap2)
                        lap2_history.(fName) = full_history.(fName)(idx_lap2);
                    end
                end
                
                % Ajusta o combustível para começar do 0 na volta 2
                if isfield(lap2_history, 'fuel_consumed_g')
                    lap2_history.fuel_consumed_g = lap2_history.fuel_consumed_g - lap2_history.fuel_consumed_g(1);
                end
                
                % Ajusta Tempo e Distância
                lap2_history.time = lap2_history.time - start_t;
                lap2_history.distance = lap2_history.distance - distancia_volta_1; 
                
                plotResults(track_geom, lap2_history, carro);
            end
        else
            plotResults(track_geom, full_history, carro);
        end
    end
end