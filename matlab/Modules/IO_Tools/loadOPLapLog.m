function [oplap_history, success] = loadOPLapLog(filename)
% loadOPLapLog - Carrega e processa dados de um log do software OPLap,
% formatando-os para serem compatíveis com o 'plotComparison'.
% (Versão corrigida usando os cabeçalhos da Linha 76)

    fprintf('Carregando log do OPLap: %s\n', filename);
    success = false;
    oplap_history = struct(); % Inicializa a struct de saída

    if ~exist(filename, 'file')
        fprintf('ERRO: Arquivo OPLap "%s" não encontrado.\n', filename);
        return;
    end

    try
        % --- 1. Leitura do CSV (Versão Final e Compatível) ---
        
        % Usamos 'HeaderLines', 75, que diz ao MATLAB para pular 75 linhas.
        % Ele lê a Linha 76 ('elapsedTime', 'speed', etc.) como os nomes
        % das colunas (cabeçalhos) e começa os dados da Linha 78.
        
        log_table = readtable(filename, ...
            'Delimiter', ';', ...
            'HeaderLines', 75); 

        
        % --- 2. Mapeamento (DE-PARA) ---
        %
        % Mapeando os nomes da Linha 76 (vistos no debug) para os nomes
        % padrão que o 'plotComparison' espera.
        
        data = struct();
        all_headers = log_table.Properties.VariableNames;
        
        try
            % Mapeamento exato (baseado no seu debug)
            data.Time            = log_table.elapsedTime;
            data.Velocity        = log_table.speed/3.6;
            data.Distance        = log_table.elapsedDistance;
            data.RPM             = log_table.engineSpeed;
            data.Gear            = log_table.gear;

            % Mapeamento parcial para nomes truncados (longitudinal/lateral G)
            
            % Para Aceleração Longitudinal
            % Encontra o nome da coluna que COMEÇA com 'longitudinalAcc'
            long_g_name = all_headers{startsWith(all_headers, 'longitudinalAcc')};
            
            % Para Aceleração Lateral
            % Encontra o nome da coluna que COMEÇA com 'lateralAccelera'
            lat_g_name = all_headers{startsWith(all_headers, 'lateralAccelera')};

            % Atribuição final usando o nome encontrado
            data.Longitudinal_G  = log_table.(long_g_name);
            data.Lateral_G       = log_table.(lat_g_name);
            
            fprintf('Mapeamento OPLap bem-sucedido.\n');

        catch ME_map
            % Se qualquer um dos nomes (ex: 'elapsedTime') falhar
            fprintf('ERRO DE MAPEAMENTO: %s\n', ME_map.message);
            fprintf('Um dos nomes de coluna esperados (ex: "elapsedTime") não foi encontrado.\n');
            success = false;
            return;
        end

        % --- Verificação de dados faltantes (Opcional, mas recomendado) ---
        required_fields = {'Time', 'Velocity', 'Distance', 'RPM', 'Gear', 'Longitudinal_G', 'Lateral_G'};
        
        for i = 1:length(required_fields)
            field = required_fields{i};
            if ~isfield(data, field)
                fprintf('AVISO: Campo requerido "%s" não foi mapeado. Usando zeros.\n', field);
                num_rows = height(log_table);
                data.(field) = zeros(num_rows, 1);
            end
        end


        % --- 3. Montagem Final (NÃO MUDE OS NOMES DOS CAMPOS AQUI) ---
        % 'plotComparison' espera exatamente estes nomes de campo.
        
        oplap_history.time = data.Time;
        oplap_history.distance = data.Distance;
        oplap_history.velocity = data.Velocity;
        oplap_history.gear = data.Gear;
        oplap_history.rpm = data.RPM;
        
        % ATENÇÃO: Converte G's para m/s^2, se necessário
        % (plotComparison espera m/s^2 no campo 'acceleration')
        oplap_history.acceleration = data.Longitudinal_G * 9.81; 
        oplap_history.lateral_accel = data.Lateral_G * 9.81;

        % Sincroniza o tempo e a distância para começar do zero
        if ~isempty(oplap_history.time)
             oplap_history.distance = oplap_history.distance - oplap_history.distance(1);
             oplap_history.time = oplap_history.time - oplap_history.time(1);
        end

        fprintf('Log OPLap processado com sucesso.\n');
        success = true;

    catch ME
        fprintf('ERRO GRAVE ao processar o arquivo OPLap: %s\n', ME.message);
        fprintf('Verifique a Seção 1 (Leitura do CSV) do loadOPLapLog.m\n');
        success = false;
    end
end