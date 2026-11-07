function [new_temp, P_in, P_out_conv, P_out_rad, P_out_cond, h] = calculateBrakeTemp(car, velocity, brake_force_total, current_temp, dt, front_load_ratio)
% calculateBrakeTemp - Calcula temperatura considerando Convecção (Trans+Rot), Radiação e Condução Radial.
%                      + Curva de Calor Específico (Cp) Variável.

    if nargin < 6
        front_load_ratio = 0.60; 
    end

    % --- 1. GERAÇÃO DE CALOR (P_in) ---
    power_total_watts = brake_force_total * velocity;
    power_front = power_total_watts * front_load_ratio;
    P_in = power_front / 2; % Por disco (metade da potência do eixo dianteiro)

    % --- 2. CÁLCULO DOS COEFICIENTES E GEOMETRIA ---
    r_tire = car.tire_radius;
    rho = car.rho;
    mu = car.air_mu;
    D = car.brake_diameter;
    k_air = car.air_k;                % Condutividade do AR (para convecção)
    k_disc = car.brake_thermalcond;   % Condutividade do DISCO (para condução)
    
    A_sup = car.brake_surface_area;   % Área exposta ao ar
    m_disco = car.brake_mass;
    
    % cp = car.brake_cp; % REMOVIDO: Agora calculamos dinamicamente no final
    
    epsilon = car.brake_emissivity;
    
    % --- 2.1 PARÂMETROS ESPECÍFICOS DE GEOMETRIA DO DISCO/CUBO (CONDUÇÃO) ---
    if isfield(car, 'brake_cond_area')
        A_cond = car.brake_cond_area; 
    else
        A_cond = A_sup * 0.05; 
    end
    
    if isfield(car, 'brake_cond_dist')
        L_path = car.brake_cond_dist; 
    else
        L_path = 0.03; 
    end

    % --- 3. CÁLCULO DE CONVECÇÃO AVANÇADO (TRANSLACIONAL + ROTACIONAL) ---
    if velocity < 0.1, velocity = 0.1; end
    
    % A) Reynolds Translacional (Vento Frontal) 
    Re_trans = (rho * velocity * D) / mu;
    
    if Re_trans <= 2.4e5
        Nu_trans = 0.70 * (Re_trans^0.55); % Laminar
    else
        Nu_trans = 0.037 * (Re_trans^0.8); % Turbulento
    end
    
    % B) Reynolds Rotacional (Efeito Ventilador do Disco) 
    omega = velocity / r_tire; 
    
    Re_rot = (rho * omega * (D/2)^2) / mu;
    
    % Correlação de Cobb & Saunders para disco girando
    Nu_rot = 0.40 * (Re_rot^0.5);
    
    % C) Nusselt Combinado (Soma Vetorial)
    Nu_total = sqrt(Nu_trans^2 + Nu_rot^2);
    
    % D) Fator de Turbulência (Cooling Factor)
    Turbulence_Factor = 3.0; 
    
    % Coeficiente de Convecção Final
    h = (Nu_total * k_air / D) * Turbulence_Factor;
    
    % --- 4. DISSIPAÇÃO DE CALOR (P_out) ---
    T_amb = 25; 
    T_hub = 60; 
    
    % Conversões Kelvin
    T_kelvin = current_temp + 273.15;
    Tamb_kelvin = T_amb + 273.15;
    Thub_kelvin = T_hub + 273.15;
    
    % A) Convecção (Ar)
    P_out_conv = h * A_sup * (current_temp - T_amb);
    
    % B) Radiação (Ambiente)
    sigma = 5.67e-8;
    P_out_rad = epsilon * sigma * A_sup * (T_kelvin^4 - Tamb_kelvin^4);

    % C) Condução (Radial para o Cubo)
    if current_temp > T_hub
        P_out_cond = (k_disc * A_cond) * (T_kelvin - Thub_kelvin) / L_path;
    else
        P_out_cond = 0; 
    end

    % Potência Total Dissipada
    P_out_total = P_out_conv + P_out_rad + P_out_cond;
    
    % --- 5. INTEGRAÇÃO NUMÉRICA COM CP VARIÁVEL ---    
    
    % Tabela de referência (Temp [C] vs Cp [J/kgK])
    T_ref = [25, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000];
    Cp_ref = [490, 515, 550, 580, 605, 630, 655, 680, 705, 725, 745];
    
    % Interpola o Cp para a temperatura atual
    % 'linear' conecta os pontos, 'extrap' garante que não crashe se passar de 1000C
    cp_dynamic = interp1(T_ref, Cp_ref, current_temp, 'linear', 'extrap');
    
    % Usa o cp_dynamic na equação principal
    rate_of_change = (P_in - P_out_total) / (m_disco * cp_dynamic);
    new_temp = current_temp + (rate_of_change * dt);
end