function plotDashboard(sim_data, car)
    % plotDashboard - Telemetria Empilhada com Data Cursor Sincronizado
    % Atualizado para incluir Energia Térmica, Carga e Combustível.
    
    fprintf('Abrindo Telemetria Empilhada (Strip Chart)...\n');
    
    % --- 1. Dados Auxiliares e Pré-Cálculos ---
    if isfield(sim_data, 'acceleration'), sim_data.long_acc_g = sim_data.acceleration/9.81; end
    if isfield(sim_data, 'lateral_accel'), sim_data.lat_acc_g = sim_data.lateral_accel/9.81; end
    if isfield(sim_data, 'velocity'), sim_data.velocity_kmh = sim_data.velocity*3.6; end
    
    % [NOVO] Cálculo de Porcentagem de Dissipação Térmica
    if isfield(sim_data, 'P_conv') && isfield(sim_data, 'P_rad') && isfield(sim_data, 'P_cond')
        P_tot = sim_data.P_conv + sim_data.P_rad + sim_data.P_cond;
        P_tot(P_tot == 0) = 1; % Evita divisão por zero onde não há energia
        
        sim_data.perc_conv = (sim_data.P_conv ./ P_tot) * 100;
        sim_data.perc_rad  = (sim_data.P_rad  ./ P_tot) * 100;
        sim_data.perc_cond = (sim_data.P_cond ./ P_tot) * 100;
    end
    
    full_data = sim_data;
    split_dist = sim_data.distance(end)/2;
    
    % --- 2. Configuração da Janela ---
    f = figure('Name', 'Telemetria FSAE (Strip Chart)', 'NumberTitle', 'off', 'Color', 'w', ...
               'Position', [100, 50, 1200, 900]); 
           
    % Definição de Variáveis Disponíveis para Plotagem
    % vars: Colunas: { Nome Visível, Nome na Struct, Unidade, Fator de Escala }
    vars = {
        'Distância', 'distance', 'm', 1;
        'Tempo', 'time', 's', 1;
        'Velocidade (km/h)', 'velocity', 'km/h', 3.6;
        'RPM', 'rpm', 'RPM', 1;
        'Marcha', 'gear', '#', 1;
        'Acel. Long. (g)', 'long_acc_g', 'g', 1;
        'Acel. Lat. (g)', 'lat_acc_g', 'g', 1;
        'Temp. Disco (°C)', 'brake_temp', '°C', 1;
        'Coef. Convecção (h)', 'h_coef', 'W/m²K', 1; 
        'Downforce', 'downforce', 'N', 1;
        'Drag', 'drag', 'N', 1;
        'Força Motor', 'engine_force', 'N', 1;
        'Força Freio', 'brake_force', 'N', 1;
        'Consumo (g)', 'fuel_consumed_g', 'g', 1;
        % --- NOVAS VARIÁVEIS ADICIONADAS ---
        'Distribuição Diant. (%)', 'load_front_dist', '%', 1;
        'Energia Conv. (W)', 'P_conv', 'W', 1;
        'Energia Rad. (W)', 'P_rad', 'W', 1;
        'Energia Cond. (W)', 'P_cond', 'W', 1;
        '% Dissip. Convecção', 'perc_conv', '%', 1;
        '% Dissip. Radiação', 'perc_rad', '%', 1;
        '% Dissip. Condução', 'perc_cond', '%', 1;
    };
    
    % Defaults Iniciais [Eixo X (Index), Eixo Y (Index)]
    % Configurado para mostrar algo novo no 4º gráfico (Distribuição de Carga)
    % Índices baseados na ordem da lista 'vars' acima.
    % 1: Dist vs Vel | 2: Dist vs RPM | 3: Dist vs Temp | 4: Dist vs Distr. Carga
    defaults = [1, 3; 1, 4; 1, 8; 1, 15]; 
    
    % --- 3. Layout: Barra Lateral de Controles ---
    panelControls = uipanel('Parent', f, 'Title', 'Configuração', ...
                            'Position', [0.01, 0.01, 0.18, 0.98], ...
                            'BackgroundColor', 'w');
                            
    % Painel da Direita para os Gráficos
    panelPlots = uipanel('Parent', f, 'Position', [0.20, 0.01, 0.79, 0.98], ...
                         'BackgroundColor', 'w', 'BorderType', 'none');
                         
    % -- Controles Gerais (Filtro) --
    uicontrol('Parent', panelControls, 'Style', 'text', 'String', 'Filtro de Volta:', ...
              'Units', 'normalized', 'Position', [0.05, 0.95, 0.9, 0.03], ...
              'BackgroundColor', 'w', 'FontWeight', 'bold', 'HorizontalAlignment', 'left');
          
    uicontrol('Parent', panelControls, 'Style', 'popupmenu', ...
              'String', {'Tudo', 'Volta 1', 'Volta 2'}, ...
              'Units', 'normalized', 'Position', [0.05, 0.92, 0.9, 0.03], ...
              'Callback', @updateLapFilter);
          
    uicontrol('Parent', panelControls, 'Style', 'text', 'String', '__________________', ...
              'Units', 'normalized', 'Position', [0.05, 0.89, 0.9, 0.02], ...
              'BackgroundColor', 'w', 'ForegroundColor', [0.8 0.8 0.8]);
              
    % -- Controles de Cada Gráfico (Gerados em Loop) --
    panels = struct('ax', {}, 'menuX', {}, 'menuY', {});
    
    start_y = 0.85;
    
    for k = 1:4
        grp_y = start_y - (k-1)*0.21;
        
        uicontrol('Parent', panelControls, 'Style', 'text', 'String', sprintf('Gráfico %d (Y vs X):', k), ...
                  'Units', 'normalized', 'Position', [0.05, grp_y, 0.9, 0.025], ...
                  'BackgroundColor', 'w', 'FontWeight', 'bold', 'HorizontalAlignment', 'left');
        
        % Menu Y
        defY = defaults(k, 2);
        if defY > size(vars, 1), defY = 3; end
        panels(k).menuY = uicontrol('Parent', panelControls, 'Style', 'popupmenu', 'String', vars(:,1), ...
            'Value', defY, 'Units', 'normalized', ...
            'Position', [0.05, grp_y-0.035, 0.9, 0.03], ...
            'Callback', {@updatePlot, k});
            
        uicontrol('Parent', panelControls, 'Style', 'text', 'String', 'vs', ...
                  'Units', 'normalized', 'Position', [0.4, grp_y-0.065, 0.2, 0.02], ...
                  'BackgroundColor', 'w');
              
        % Menu X
        panels(k).menuX = uicontrol('Parent', panelControls, 'Style', 'popupmenu', 'String', vars(:,1), ...
            'Value', defaults(k, 1), 'Units', 'normalized', ...
            'Position', [0.05, grp_y-0.10, 0.9, 0.03], ...
            'Callback', {@updatePlot, k});
            
        % === CRIAÇÃO DO EIXO NO PAINEL DE GRÁFICOS ===
        panels(k).ax = subplot(4, 1, k, 'Parent', panelPlots);
        grid(panels(k).ax, 'on'); hold(panels(k).ax, 'on'); box(panels(k).ax, 'on');
    end
    
    linkaxes([panels.ax], 'x');
    
    % --- Configuração do Data Cursor Customizado ---
    dcm = datacursormode(f);
    set(dcm, 'UpdateFcn', @customTooltip, 'Enable', 'on');
    
    % Inicializa
    current_data = full_data;
    refreshAllPlots();
    
    % =====================================================================
    %                           FUNÇÕES AUXILIARES
    % =====================================================================
    function updateLapFilter(src, ~)
        val = src.Value;
        if val == 1, current_data = full_data;
        elseif val == 2, current_data = filterStruct(full_data, full_data.distance <= split_dist);
        elseif val == 3
            current_data = filterStruct(full_data, full_data.distance > split_dist);
            % Ajusta a distância para começar do zero na volta 2
            if ~isempty(current_data.distance)
                current_data.distance = current_data.distance - current_data.distance(1); 
            end
        end
        refreshAllPlots();
    end

    function new_s = filterStruct(s, mask)
        fields = fieldnames(s);
        for i = 1:length(fields)
            if isnumeric(s.(fields{i})) && length(s.(fields{i})) == length(mask)
                new_s.(fields{i}) = s.(fields{i})(mask);
            else, new_s.(fields{i}) = s.(fields{i}); end
        end
    end

    function refreshAllPlots()
        for i=1:4, updatePlot([],[],i); end
    end

    function updatePlot(~, ~, idx)
        p = panels(idx);
        idxX = p.menuX.Value; scaleX = vars{idxX, 4};
        idxY = p.menuY.Value; scaleY = vars{idxY, 4};
        
        fieldX = vars{idxX, 2}; fieldY = vars{idxY, 2};
        
        if isfield(current_data, fieldX) && isfield(current_data, fieldY)
            x = current_data.(fieldX) * scaleX;
            y = current_data.(fieldY) * scaleY;
            
            % Limpa e plota
            cla(p.ax);
            plot(p.ax, x, y, 'b-', 'LineWidth', 1.2);
            
            % Título lateral
            ylabel(p.ax, vars{idxY, 1}, 'FontWeight', 'bold', 'FontSize', 9);
            
            % Eixo X apenas no último
            if idx == 4
                xlabel(p.ax, vars{idxX, 1}, 'FontWeight', 'bold');
            else
                set(p.ax, 'XTickLabel', []); 
            end
            
            grid(p.ax, 'on'); xlim(p.ax, 'auto');
        end
    end

    % --- FUNÇÃO DO DATA CURSOR SINCRONIZADO ---
    function txt = customTooltip(~, event_obj)
        pos = get(event_obj, 'Position'); 
        target_x = pos(1);
        
        txt = {sprintf('X: %.2f', target_x)};
        
        % Remove linhas antigas
        lines = findobj(f, 'Tag', 'CursorLine');
        delete(lines);
        
        for i = 1:4
            ax = panels(i).ax;
            yl = ylim(ax);
            % Desenha linha vertical em TODOS os gráficos
            line(ax, [target_x target_x], yl, 'Color', 'r', 'LineStyle', '--', 'LineWidth', 1, 'Tag', 'CursorLine');
            
            idxX = panels(i).menuX.Value; fieldX = vars{idxX, 2}; scaleX = vars{idxX, 4};
            idxY = panels(i).menuY.Value; fieldY = vars{idxY, 2}; scaleY = vars{idxY, 4};
            
            if isfield(current_data, fieldX) && isfield(current_data, fieldY)
                X_vec = current_data.(fieldX) * scaleX;
                Y_vec = current_data.(fieldY) * scaleY;
                
                % Interpola para achar o Y exato no X clicado
                % Usa 'nearest' ou 'linear' dependendo da preferência. 'Linear' é mais suave.
                if length(X_vec) > 1
                    val_y = interp1(X_vec, Y_vec, target_x, 'linear');
                else
                    val_y = Y_vec(1);
                end
                
                nameY = vars{idxY, 1};
                unitY = vars{idxY, 3};
                txt{end+1} = sprintf('%s: %.2f %s', nameY, val_y, unitY);
            end
        end
    end
end