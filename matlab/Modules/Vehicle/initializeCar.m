function car = initializeCar()
    % initializeCar - Define e retorna todos os parâmetros fixos do carro.

    % --- Dados Gerais ---
    car.mass_only = 220;            % Massa do veículo [kg]
    car.pilot_mass = 75;            % Massa do piloto [kg]
    car.mass = car.mass_only + car.pilot_mass; % Massa total [kg]
    car.g = 9.81;                   % Aceleração da gravidade [m/s^2]

    % --- Dados Aerodinâmicos (mantendo a estrutura original) ---
    car.frontalArea = 1.1;          % Área frontal [m^2] (mantida da versão anterior)
    car.rho = 1.18836;              % Densidade do ar [kg/m^3]
    % Valores de cl e cd calculados ClA/A CdA/A
    car.cl = 3.03 / car.frontalArea; % Coeficiente de sustentação (lift)
    car.cd = 1.28 / car.frontalArea; % Coeficiente de arrasto (drag)
    car.cpaero = 50;
    
    % --- Dados de Dinâmica / Pneus ---
    car.wheelbase = 1.550;          % Distância entre eixos [m]
    car.tire_radius = 228.6 / 1000; % Raio do pneu [m] (convertido de mm)
    car.weight_dist_rear_percent = 50; % Distribuição de peso no eixo traseiro [%]
    car.cg_height = 0.312;          % Altura do Centro de Gravidade [m]
    
    % Mantidos da versão anterior para compatibilidade
    car.track_front_m = 1.2;        % Bitola dianteira [m]
    car.track_rear_m = 1.15;        % Bitola traseira [m]
    car.mux_static = 2.1;           % Coeficiente de atrito longitudinal ESTÁTICO
    car.mux_kinetic = 1.3;          % Coeficiente de atrito longitudinal CINÉTICO
    car.muy = 1.2;                  % Coeficiente de atrito lateral

    % --- Dados de Motor e Transmissão (mantendo a estrutura original) ---
    car.overall_reduction = 2.111 * 3.576; % Redução total (primária * final)
    car.gear_ratios = [1.938, 1.556, 1.348, 1.208, 1.095]; % Relações de marcha (5 marchas)
    car.powertrain_efficiency = 0.95; % Eficiência da transmissão
    
    car.shiftTime = 120 / 1000;     % Tempo de troca de marcha [s] (convertido de ms)
    car.shiftRPM_vec = [12000,12000,12000,12000,12000];          % RPM para troca de marcha
    car.launchRPM = 8600;           % RPM do "two-step" launch control
    car.downshiftRPM = 11500;       % RPM mínimo para considerar uma redução de marcha 
    
    % Carrega a curva de Torque e Potência a partir da função dataEngine
    [car.TorqueInterpol, car.PowerInterpol, car.rpm_min, car.rpm_max, car.bsfc_map] = dataEngine();

    
    % --- Freios e Análise Térmica --- 
    % car.brake_torque = ;          % Torque de frenagem máximo [N.m]
    car.brake_efficiency = 0.99;    % Eficiência do sistema de freio
    
    % Propriedades do Disco
    car.brake_density = 8000;       % Massa Específica do Disco (Aço SAE 304) [kg/m^3]
    car.brake_mass = 0.458;         % Massa de UM disco [kg]
    
    car.brake_thickness = 0.004;      % Espessura do Disco de Freio [m]
    car.brake_diameter = 0.175;       % Diâmetro externo do disco [m]
    car.brake_surface_area = 0.03856; % Área de troca térmica (faces + rasgos do disco sólido) [m^2]

    % Propriedades Térmicas do Disco
    car.brake_cp = 500;             % Calor específico (Aço SAE 304) [J/(kg*K)]
    car.brake_thermalcond = 16;     % Condutividade Térmica (Aço SAE 304) [W/m*K]
    car.brake_emissivity = 0.55;    % Emissividade para Radiação (0.55 é conservador p/ aço oxidado/quente)

    num_bobbins = 5; % Número de rebites flutuantes
    diametro_bobbin = 0.009; % Diâmetro dos rebites flutuantes
    car.brake_cond_area = num_bobbins * (pi * (diametro_bobbin/2)^2); % Área da seção transversal do caminho do disco para o cubo
    car.brake_cond_dist = 0.0282; % Distância radial (Centro da Pastilha -> Centro do Rebite que prende o disco no cubo)

    % Propriedades do Ar (para cálculo de Reynolds e Convecção)
    car.air_k = 0.026;              % Condutividade térmica do ar [W/(m*K)] @ 25°C
    car.air_mu = 1.8e-5;            % Viscosidade dinâmica do ar [kg/(m*s)] @ 25°C

end