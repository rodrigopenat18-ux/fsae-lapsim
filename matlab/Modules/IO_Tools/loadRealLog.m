function [real_log_history, success] = loadRealLog(trackType)
% loadRealLog - (Versão Final) Carrega, processa e formata dados de um log,
% usando uma lógica robusta de mapeamento de colunas parciais.

    % Determina o nome do arquivo padrão
    if trackType == 1      
        filename = 'log_autocross.csv';
    elseif trackType == 2  
        filename = 'log_skidpad.csv';
    elseif trackType == 3 
        filename = 'log_acceleration.csv';
    elseif trackType == 4
        filename = 'log_autox.csv';
    else
        fprintf('Tipo de pista inválido para carregar log.\n');
        real_log_history = struct(); 
        success = false; 
        return;
    end

    if ~exist(filename, 'file')
        fprintf('\nAVISO: Arquivo de log "%s" não encontrado.\n', filename);
        real_log_history = struct(); 
        success = false; 
        return;
    end

    fprintf('Log real "%s" encontrado. Processando dados...\n', filename);
    
    opts = detectImportOptions(filename);
    opts.VariableNamingRule = 'preserve';
    log_table = readtable(filename, opts);
    
    file_headers = log_table.Properties.VariableNames;

    % --- LÓGICA DE MAPEAMENTO ROBUSTA (usando "startsWith") ---
    header_map = containers.Map();
    % Adicionamos prefixos curtos e confiáveis à lista
    header_map('Time')           = {'TIME', 'TEMPO'};
    header_map('Velocity')       = {'Velocidade_de_t', 'VELOCIDADE'};
    header_map('RPM')            = {'RPM'};
    header_map('Longitudinal_G') = {'Força_G_acelera', 'ACELERACAO_LONGITUDINAL'};
    header_map('Lateral_G')      = {'Força_G_lateral', 'ACELERACAO_LATERAL'};
    header_map('Gear')           = {'MARCHA', 'Gear'};
    header_map('Distance')       = {'DISTANCIA', 'Distance'};

    data = struct();
    map_keys = keys(header_map);

    for i = 1:length(map_keys)
        standard_name = map_keys{i};
        aliases = header_map(standard_name);
        
        % Itera sobre os cabeçalhos do arquivo para encontrar uma correspondência parcial
        for j = 1:length(file_headers)
            header_in_file = file_headers{j};
            % Verifica se o cabeçalho do arquivo COMEÇA com algum dos nossos apelidos
            if any(startsWith(header_in_file, aliases, 'IgnoreCase', true))
                data.(standard_name) = log_table.(header_in_file);
                fprintf('   -> Campo "%s" mapeado com sucesso a partir da coluna "%s".\n', standard_name, header_in_file);
                break; % Para de procurar por este campo padrão
            end
        end
    end
    
    % --- VERIFICAÇÃO E CÁLCULO DE DADOS FALTANTES ---
    if ~isfield(data, 'Distance')
        fprintf("   -> Coluna 'Distance' não encontrada. Calculando...\n");
        if isfield(data, 'Time') && isfield(data, 'Velocity')
            vel_ms = data.Velocity / 3.6;
            time_diff = diff([0; data.Time]);
            dist_step = vel_ms(1:end-1) .* time_diff(2:end);
            data.Distance = [0; cumsum(dist_step)];
        else
            fprintf("ERRO: Não foi possível calcular a distância. Colunas 'Time' ou 'Velocity' não foram mapeadas corretamente.\n");
            real_log_history = struct(); success = false; return;
        end
    end
    
    required_fields = {'Time', 'Distance', 'Velocity', 'Gear', 'RPM', 'Longitudinal_G', 'Lateral_G'};
    for i=1:length(required_fields)
        if ~isfield(data, required_fields{i})
            data.(required_fields{i}) = zeros(height(log_table), 1);
        end
    end

    % --- MONTAGEM FINAL ---
    real_log_history = struct();
    real_log_history.time = data.Time;
    real_log_history.distance = data.Distance;
    real_log_history.velocity = data.Velocity / 3.6;
    real_log_history.gear = data.Gear;
    real_log_history.rpm = data.RPM;
    real_log_history.acceleration = data.Longitudinal_G * 9.81;
    real_log_history.lateral_accel = data.Lateral_G * 9.81;
    
    success = true;
    fprintf('Dados do log real processados com sucesso.\n');
end