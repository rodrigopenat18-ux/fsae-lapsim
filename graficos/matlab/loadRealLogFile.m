function [real_log_history, success] = loadRealLogFile(filename)
%LOADREALLOGFILE Lê um log de aquisição do carro real (versão corrigida do loadRealLog.m).
%
%   [hist, ok] = loadRealLogFile('dados/privado/log_autox.csv')
%
%   Mesma lógica do matlab/Modules/IO_Tools/loadRealLog.m, com duas correções
%   de leitura (não mudam a física):
%     - recebe o caminho do arquivo (o original escolhia o nome pelo tipo de
%       pista e procurava "log_autocross.csv", que não existe);
%     - compara os nomes das colunas sem acentos, então "Distância" casa com
%       "DISTANCIA" (no original a distância era sempre recalculada).
%   Saída no formato esperado pelo plotComparison.m (SI: m/s, m/s²).

    real_log_history = struct();
    success = false;
    if ~isfile(filename)
        fprintf('\nAVISO: Arquivo de log "%s" não encontrado.\n', filename);
        return;
    end
    fprintf('Log real "%s" encontrado. Processando dados...\n', filename);

    opts = detectImportOptions(filename);
    opts.VariableNamingRule = 'preserve';
    log_table = readtable(filename, opts);
    file_headers = log_table.Properties.VariableNames;

    header_map = containers.Map();
    header_map('Time')           = {'TIME', 'TEMPO'};
    header_map('Velocity')       = {'Velocidade_de_t', 'VELOCIDADE'};
    header_map('RPM')            = {'RPM'};
    header_map('Longitudinal_G') = {'Forca_G_acelera', 'ACELERACAO_LONGITUDINAL'};
    header_map('Lateral_G')      = {'Forca_G_lateral', 'ACELERACAO_LATERAL'};
    header_map('Gear')           = {'MARCHA', 'Gear'};
    header_map('Distance')       = {'DISTANCIA', 'Distance'};

    data = struct();
    map_keys = keys(header_map);
    for i = 1:length(map_keys)
        standard_name = map_keys{i};
        aliases = header_map(standard_name);
        for j = 1:length(file_headers)
            header_in_file = file_headers{j};
            if any(startsWith(strip_accents(header_in_file), aliases, 'IgnoreCase', true))
                data.(standard_name) = log_table.(header_in_file);
                fprintf('   -> Campo "%s" mapeado a partir da coluna "%s".\n', standard_name, header_in_file);
                break;
            end
        end
    end

    if ~isfield(data, 'Distance')
        fprintf("   -> Coluna 'Distance' não encontrada. Calculando...\n");
        if isfield(data, 'Time') && isfield(data, 'Velocity')
            vel_ms = data.Velocity / 3.6;
            time_diff = diff([0; data.Time]);
            dist_step = vel_ms(1:end-1) .* time_diff(2:end);
            data.Distance = [0; cumsum(dist_step)];
        else
            fprintf("ERRO: colunas de tempo ou velocidade não encontradas.\n");
            return;
        end
    end

    required_fields = {'Time', 'Distance', 'Velocity', 'Gear', 'RPM', 'Longitudinal_G', 'Lateral_G'};
    for i = 1:length(required_fields)
        if ~isfield(data, required_fields{i})
            data.(required_fields{i}) = zeros(height(log_table), 1);
        end
    end

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

function s = strip_accents(s)
    s = char(s);
    from = 'áàâãäéèêëíìîïóòôõöúùûüçÁÀÂÃÄÉÈÊËÍÌÎÏÓÒÔÕÖÚÙÛÜÇ';
    to   = 'aaaaaeeeeiiiiooooouuuucAAAAAEEEEIIIIOOOOOUUUUC';
    for k = 1:numel(from)
        s(s == from(k)) = to(k);
    end
end
