function plotInteractiveTrack(track_geom, sim_data, car)
    % plotInteractiveTrack - Janela Combinada: Diagrama GG (Esq) + Mapa Pista (Dir)
    % Atualizado para incluir Carga, Energia Térmica e Consumo.
    
    fprintf('Gerando Visualização Combinada (GG + Pista)...\n');
    
    % --- 1. Preparação dos Dados ---
    if isfield(sim_data, 'acceleration'), long_g = sim_data.acceleration / 9.81; else, long_g = []; end
    if isfield(sim_data, 'lateral_accel'), lat_g = sim_data.lateral_accel / 9.81; else, lat_g = []; end
    if isfield(sim_data, 'velocity'), vel_kmh = sim_data.velocity * 3.6; else, vel_kmh = []; end
    
    % Mapeamento Geométrico (para a Pista)
    geo_dx = diff(track_geom.X);
    geo_dy = diff(track_geom.Y);
    geo_dist_steps = sqrt(geo_dx.^2 + geo_dy.^2);
    geo_dist_total = [0, cumsum(geo_dist_steps)];
    track_length = geo_dist_total(end);
    
    if isfield(sim_data, 'distance'), d_sim = sim_data.distance; else, d_sim = zeros(size(sim_data.velocity)); end
    
    % Interpolação para encontrar X,Y baseados na distância percorrida
    sim_dist_mapped = mod(d_sim, track_length);
    X_sim = interp1(geo_dist_total, track_geom.X, sim_dist_mapped, 'linear', 'extrap');
    Y_sim = interp1(geo_dist_total, track_geom.Y, sim_dist_mapped, 'linear', 'extrap');
    
    % --- 2. Configuração da Figura ---
    f = figure('Name', 'Analise de Performance FSAE', 'NumberTitle', 'off', 'Color', 'w', ...
               'Position', [50, 50, 1400, 600]); 
    
    % =====================================================================
    % PAINEL 1 (ESQUERDA): DIAGRAMA GG
    % =====================================================================
    axGG = subplot(1, 2, 1);
    hold(axGG, 'on'); grid(axGG, 'on'); box(axGG, 'on');
    title(axGG, 'Diagrama GG (Cor = Velocidade)', 'FontSize', 12);
    xlabel(axGG, 'Aceleração Lateral [g]', 'FontWeight', 'bold');
    ylabel(axGG, 'Aceleração Longitudinal [g]', 'FontWeight', 'bold');
    
    if ~isempty(lat_g) && ~isempty(long_g)
        scatter(axGG, lat_g, long_g, 15, vel_kmh, 'filled', 'MarkerFaceAlpha', 0.4);
        cgg = colorbar(axGG, 'Location', 'westoutside');
        cgg.Label.String = 'Velocidade [km/h]';
    end
    
    % Limite Teórico
    if isfield(car, 'mu'), mu = car.mu; else, mu = 1.5; end
    theta = linspace(0, 2*pi, 100);
    plot(axGG, mu*cos(theta), mu*sin(theta), 'r--', 'LineWidth', 2, 'DisplayName', ['Limite \mu=' num2str(mu)]);
    legend(axGG, 'Location', 'best');
    
    axis(axGG, 'equal');
    lim = max(2.5, mu+0.5);
    axis(axGG, [-lim lim -lim lim]);
    xline(axGG, 0, 'Color', [0.5 0.5 0.5]); yline(axGG, 0, 'Color', [0.5 0.5 0.5]);
    
    % =====================================================================
    % PAINEL 2 (DIREITA): MAPA DA PISTA INTERATIVO
    % =====================================================================
    axMap = subplot(1, 2, 2);
    axis(axMap, 'equal'); grid(axMap, 'on'); hold(axMap, 'on');
    xlabel(axMap, 'X [m]'); ylabel(axMap, 'Y [m]');
    
    % Fundo da pista
    plot(axMap, track_geom.X, track_geom.Y, '-', 'Color', [0.8 0.8 0.8], 'LineWidth', 1);
    
    % Scatter Inicial
    hPlotMap = scatter(axMap, X_sim, Y_sim, 20, vel_kmh, 'filled');
    
    cMap = colorbar(axMap);
    cMap.Label.String = 'Velocidade [km/h]';
    colormap(f, turbo); 
    title(axMap, 'Track Map: Velocidade (km/h)');
    
    % --- Controles da Dropdown ---
    % { Nome Visível, Nome na Struct, Unidade }
    vars = {
        'Velocidade',           'velocity',           'm/s';
        'Velocidade (km/h)',    'velocity',           'km/h'; 
        'RPM do Motor',         'rpm',                'RPM';
        'Marcha',               'gear',               '#';
        'Aceleração Long.',     'acceleration',       'm/s²';
        'Temp. Freio (°C)',     'brake_temp',         '°C';
        'Coef. Convecção (h)',  'h_coef',             'W/m²K'; 
        'Arrasto (Drag)',       'drag',               'N';
        'Downforce',            'downforce',          'N';
        'Força Motor',          'engine_force',       'N';
        'Força Freio',          'brake_force',        'N';
        % --- NOVOS CAMPOS ---
        'Dist. Carga Diant.',   'load_front_dist',    '%';
        'Pot. Convecção',       'P_conv',             'W';
        'Pot. Radiação',        'P_rad',              'W';
        'Pot. Condução',        'P_cond',             'W';
        'Consumo Acumulado',    'fuel_consumed_g',    'g';
    };
    
    uicontrol('Parent', f, 'Style', 'text', 'String', 'Variável do Mapa:', ...
              'Units', 'normalized', 'Position', [0.55, 0.92, 0.1, 0.03], ...
              'BackgroundColor', 'w', 'FontWeight', 'bold', 'HorizontalAlignment', 'right');
          
    uicontrol('Parent', f, 'Style', 'popupmenu', 'String', vars(:,1), ...
              'Units', 'normalized', 'Position', [0.66, 0.92, 0.15, 0.04], ...
              'Value', 2, 'Callback', @updateMap);
          
    function updateMap(source, ~)
        idx = source.Value;
        var_name = vars{idx, 2};
        unit = vars{idx, 3};
        label = vars{idx, 1};
        
        if isfield(sim_data, var_name)
            data = sim_data.(var_name);
            
            % Tratamento especial para conversão de unidade apenas quando o label pede
            if strcmp(label, 'Velocidade (km/h)')
                data = data * 3.6; 
            end
            
            set(hPlotMap, 'CData', data);
            cMap.Label.String = sprintf('%s [%s]', label, unit);
            title(axMap, ['Track Map: ' label]);
            
            if max(data) > min(data)
                caxis(axMap, [min(data), max(data)]); 
            end
        else
            % Fallback caso o dado não exista (ex: rodou simulação antiga)
            warning('Variável "%s" não encontrada.', var_name);
            title(axMap, [label ' (DADO NÃO DISPONÍVEL)']);
        end
    end
end