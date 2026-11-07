function [total_time, final_history] = calculate_Straight(car, straight_length_m, initial_velocity_ms, target_velocity_ms, slope, trackType, initial_brake_temp)
    
    if nargin < 7
        initial_brake_temp = 25;
    end
    dt = 0.01;
    
    % Distribuição estática para uso padrão
    static_front_ratio = (1 - car.weight_dist_rear_percent/100);
    
    % =========================================================================
    % MODO 1: ACELERAÇÃO MÁXIMA (Target infinito)
    % =========================================================================
    if isinf(target_velocity_ms)
        pos = 0; vel = initial_velocity_ms; time = 0;
        current_fuel_g = 0; 
        
        current_temp = initial_brake_temp;
        current_gear = findInitialGear(car, vel);
        [motor_rpm, ~] = updateEngineState(car, vel, current_gear, (initial_velocity_ms < 1.0), 0, 0);
        
        % --- ALTERAÇÃO 1: Inicialização do Histórico Expandido ---
        history = struct('time', time, 'position', pos, 'velocity', vel, ...
                         'acceleration', 0, 'gear', current_gear, 'rpm', motor_rpm, ...
                         'drag', 0, 'downforce', 0, 'rolling_resistance', 0, ...
                         'slope_force', 0, 'engine_force', 0, 'brake_force', 0, ...
                         'brake_temp', current_temp, 'h_coef', 0, ...
                         'fuel_consumed_g', current_fuel_g, ...
                         'P_conv', 0, 'P_rad', 0, 'P_cond', 0, ...      % <--- NOVO
                         'load_front_dist', static_front_ratio*100);    % <--- NOVO
                         
        shift_status = struct('isShifting',false,'timer',0);
        launch_mode = (initial_velocity_ms < 1.0);
        launch_timer = 0;
        
        while pos < straight_length_m
            [engine_force_calc, drag, tyre_resistance, slope_force, current_gear, shift_status, downforce] = ...
                calculateForces(car, vel, slope, current_gear, true, shift_status, dt, time);
            
            [pos_new, vel_new, acc] = carAccelerate(car, engine_force_calc, drag, tyre_resistance, slope_force, vel, pos, dt, slope, downforce);
            
            % --- BSFC ---
            torque_motor = car.TorqueInterpol(motor_rpm);
            pot_inst_kW = (torque_motor * motor_rpm) / 9549; 
            bsfc_inst = car.bsfc_map(motor_rpm);
            fuel_step = (bsfc_inst * pot_inst_kW * dt) / 3600; 
            current_fuel_g = current_fuel_g + fuel_step;
            
            % --- ALTERAÇÃO 2: Cálculo Dinâmico de Carga (Mesmo em Aceleração) ---
            % Calcula o squat para o gráfico ficar bonito
            [Normal_N, ~] = calculateLoadTransfer(car, acc/car.g, 0, slope, vel);
            if sum(Normal_N) > 0
                dynamic_ratio = (Normal_N(1) + Normal_N(2)) / sum(Normal_N);
            else
                dynamic_ratio = static_front_ratio;
            end
            
            % --- ALTERAÇÃO 3: Chamada Térmica Expandida ---
            [current_temp, ~, P_conv_val, P_rad_val, P_cond_val, h_val] = ...
                calculateBrakeTemp(car, vel, 0, current_temp, dt, dynamic_ratio);
            
            pos = pos_new; vel = vel_new; time = time + dt;
            [motor_rpm, launch_mode] = updateEngineState(car, vel, current_gear, launch_mode, launch_timer, dt); 
            
            % Update History
            history.time(end+1) = time; history.position(end+1) = pos; history.velocity(end+1) = vel;
            history.acceleration(end+1) = acc; history.gear(end+1) = current_gear; history.rpm(end+1) = motor_rpm;
            history.drag(end+1) = drag; history.downforce(end+1) = downforce;
            history.rolling_resistance(end+1) = tyre_resistance; history.slope_force(end+1) = slope_force;
            history.engine_force(end+1) = engine_force_calc; history.brake_force(end+1) = 0;
            history.brake_temp(end+1) = current_temp;
            history.h_coef(end+1) = h_val;
            history.fuel_consumed_g(end+1) = current_fuel_g;
            
            % --- ALTERAÇÃO 4: Salvando novos dados ---
            history.P_conv(end+1) = P_conv_val;
            history.P_rad(end+1) = P_rad_val;
            history.P_cond(end+1) = P_cond_val;
            history.load_front_dist(end+1) = dynamic_ratio * 100;
        end
        total_time = time;
        final_history = history;
        return;
    end
    
    % =========================================================================
    % MODO 2: VELOCIDADE ALVO (Bisseção Otimizada)
    % =========================================================================
    low_bp = 0.0;                   
    high_bp = straight_length_m;    
    precision_m = 0.05;  
    vel_tolerance = 0.1; 
    max_iterations = 100; 
    total_time_safe = -1;
    final_history_safe = struct();
    
    for iter_count = 1:max_iterations
        braking_point = (low_bp + high_bp) / 2.0;
        pos = 0; vel = initial_velocity_ms; time = 0; current_fuel_g = 0;
        
        current_temp = initial_brake_temp;
        current_gear = findInitialGear(car, vel);
        [motor_rpm, launch_mode] = updateEngineState(car, vel, current_gear, (initial_velocity_ms < 1.0), 0, 0);
        
        % --- ALTERAÇÃO 1: Inicialização do Histórico Expandido ---
        history = struct('time', time, 'position', pos, 'velocity', vel, ...
                         'acceleration', 0, 'gear', current_gear, 'rpm', motor_rpm, ...
                         'drag', 0, 'downforce', 0, 'rolling_resistance', 0, ...
                         'slope_force', 0, 'engine_force', 0, 'brake_force', 0, ...
                         'brake_temp', current_temp, 'h_coef', 0, ...
                         'fuel_consumed_g', current_fuel_g, ...
                         'P_conv', 0, 'P_rad', 0, 'P_cond', 0, ...      % <--- NOVO
                         'load_front_dist', static_front_ratio*100);    % <--- NOVO
                         
        shift_status = struct('isShifting', false, 'timer', 0);
        launch_timer = 0;
        simulation_running = true;
        stopped_early = false; 
        
        while simulation_running
            isAccelerating = (pos < braking_point);
            
            [engine_force_calc, drag, tyre_resistance, slope_force, current_gear, shift_status, downforce] = ...
                calculateForces(car, vel, slope, current_gear, isAccelerating, shift_status, dt, time);
            
            curr_eng = 0; curr_brk = 0; fuel_step = 0;
            dynamic_ratio = static_front_ratio; 

            if isAccelerating
                curr_eng = engine_force_calc;
                [pos_new, vel_new, acc] = carAccelerate(car, engine_force_calc, drag, tyre_resistance, slope_force, vel, pos, dt, slope, downforce);
                
                % BSFC
                torque_motor = car.TorqueInterpol(motor_rpm);
                pot_inst_kW = (torque_motor * motor_rpm) / 9549;
                bsfc_inst = car.bsfc_map(motor_rpm);
                fuel_step = (bsfc_inst * pot_inst_kW * dt) / 3600;
                
                % --- ALTERAÇÃO 2a: Cálculo Carga em Aceleração ---
                [Normal_N, ~] = calculateLoadTransfer(car, acc/car.g, 0, slope, vel);
                if sum(Normal_N) > 0
                    dynamic_ratio = (Normal_N(1) + Normal_N(2)) / sum(Normal_N);
                end

            else
                % FRENAGEM
                brake_force_calc = calculateBrakeForce(car);
                curr_brk = brake_force_calc;
                
                [pos_new, vel_new, acc, curr_brk] = carBrake(car, brake_force_calc, drag, tyre_resistance, slope_force, vel, pos, dt, slope, downforce);
                
                % --- ALTERAÇÃO 2b: Cálculo Carga em Frenagem (Já existia, só garanti o uso) ---
                [Normal_N, ~] = calculateLoadTransfer(car, acc/car.g, 0, slope, vel);
                F_front = Normal_N(1) + Normal_N(2);
                F_total = sum(Normal_N);
                
                if F_total > 0
                    dynamic_ratio = F_front / F_total;
                else
                    dynamic_ratio = 0.5; 
                end
            end
            
            current_fuel_g = current_fuel_g + fuel_step;
            
            % --- ALTERAÇÃO 3: Chamada Térmica Expandida ---
            [current_temp, ~, P_conv_val, P_rad_val, P_cond_val, h_val] = ...
                calculateBrakeTemp(car, vel, curr_brk, current_temp, dt, dynamic_ratio);
            
            pos = pos_new; vel = vel_new; time = time + dt;
            [motor_rpm, launch_mode] = updateEngineState(car, vel, current_gear, launch_mode, launch_timer, dt);
            
            history.time(end+1)=time; history.position(end+1)=pos; history.velocity(end+1)=vel;
            history.acceleration(end+1)=acc; history.gear(end+1)=current_gear; history.rpm(end+1)=motor_rpm;
            history.drag(end+1)=drag; history.downforce(end+1)=downforce; history.rolling_resistance(end+1)=tyre_resistance;
            history.slope_force(end+1)=slope_force; history.engine_force(end+1)=curr_eng; history.brake_force(end+1)=curr_brk;
            history.brake_temp(end+1) = current_temp; history.h_coef(end+1) = h_val;
            history.fuel_consumed_g(end+1) = current_fuel_g;
            
            % --- ALTERAÇÃO 4: Salvando novos dados ---
            history.P_conv(end+1) = P_conv_val;
            history.P_rad(end+1) = P_rad_val;
            history.P_cond(end+1) = P_cond_val;
            history.load_front_dist(end+1) = dynamic_ratio * 100;

            if pos >= straight_length_m
                simulation_running = false; 
            elseif vel <= 0.1 && ~isAccelerating 
                simulation_running = false;
                stopped_early = true;
            end
        end
        
        final_vel = vel;
        diff_vel = final_vel - target_velocity_ms;
        
        if stopped_early
            low_bp = braking_point;
            if total_time_safe == -1 
                 total_time_safe = time; final_history_safe = history;
            end
        elseif final_vel <= target_velocity_ms
            low_bp = braking_point;
            total_time_safe = time;
            final_history_safe = history;
            if abs(diff_vel) < vel_tolerance, break; end
        else
            high_bp = braking_point;
        end
        if (high_bp - low_bp) < precision_m, break; end
    end
    
    if total_time_safe == -1
        total_time = time; final_history = history;
    else
        total_time = total_time_safe; final_history = final_history_safe;
    end
end