function plotResults(track_geom, history, car)
% plotResults - Plota um conjunto completo de resultados da simulação.
    
    % --- Garantir que X, Y, Z sejam vetores coluna ---
    track_geom.X = track_geom.X(:);
    track_geom.Y = track_geom.Y(:);
    track_geom.Z = track_geom.Z(:);

    %% --- JANELA 1: GRÁFICOS DE PERFORMANCE (vs. Tempo) ---
    % Aumentamos a janela para 5 subplots para caber o combustível
    figure('Name', 'Resultados da Simulação vs. Tempo', 'Position', [50, 50, 1000, 950]);
    
    % 1. Velocidade vs Tempo
    subplot(5,1,1);
    plot(history.time, history.velocity * 3.6, 'b-', 'LineWidth', 1.5);
    title('Velocidade vs. Tempo');
    ylabel('km/h');
    grid on; xlim([0, history.time(end)]);

    % 2. Acelerações Longitudinal e Lateral vs Tempo
    subplot(5,1,2);
    hold on;
    plot(history.time, history.acceleration / 9.81, 'r-', 'LineWidth', 1.5);
    plot(history.time, history.lateral_accel / 9.81, 'b-', 'LineWidth', 1.5);
    hold off;
    title('Acelerações vs. Tempo');
    ylabel('Aceleração (g)');
    grid on; xlim([0, history.time(end)]);
    legend('Longitudinal', 'Lateral', 'Location', 'best');

    % 3. Rotação do Motor (RPM) vs Tempo
    subplot(5,1,3);
    plot(history.time, history.rpm, 'g-', 'LineWidth', 1.5);
    hold on;
    yline(car.shiftRPM_vec, 'r--', 'LineWidth', 1, 'Label', 'Troca');
    hold off;
    title('Rotação do Motor (RPM)');
    ylabel('RPM');
    grid on; xlim([0, history.time(end)]);
    ylim([car.rpm_min - 500, car.rpm_max + 500]);

    % 4. CONSUMO ACUMULADO (NOVO)
    subplot(5,1,4);
    if isfield(history, 'fuel_consumed_g')
        plot(history.time, history.fuel_consumed_g, 'Color', [0.4660 0.6740 0.1880], 'LineWidth', 2);
        title('Consumo de Combustível Acumulado');
        ylabel('Gramas (g)');
        grid on; xlim([0, history.time(end)]);
    else
        text(0.5, 0.5, 'Dados de Combustível não encontrados', 'HorizontalAlignment', 'center');
    end

    % 5. Marcha vs Tempo
    subplot(5,1,5);
    stairs(history.time, history.gear, 'k-', 'LineWidth', 2);
    title('Marcha Utilizada');
    ylabel('Marcha');
    xlabel('Tempo (s)');
    grid on; xlim([0, history.time(end)]);
    ylim([0 length(car.gear_ratios) + 1]);

    %% --- JANELA 2: ANÁLISE DE DADOS (vs. Distância e G-G) ---
    % (Sua Janela 2 permanece idêntica, apenas mantive a estrutura para você copiar)
    figure('Name', 'Análise de Performance e Mapa da Pista', 'Position', [100, 100, 1600, 900]);
    
    % Velocidade vs Distância
    ax1 = subplot(2,2,1);
    plot(ax1, history.distance, history.velocity * 3.6, 'm-', 'LineWidth', 2);
    title(ax1, 'Velocidade vs. Distância');
    xlabel(ax1, 'Distância (m)');
    ylabel(ax1, 'km/h');
    grid(ax1, 'on'); xlim(ax1, [0, history.distance(end)]);
    
    % Diagrama G-G
    ax2 = subplot(2,2,2);
    scatter(ax2, history.lateral_accel/9.81, history.acceleration/9.81, 15, history.velocity*3.6, 'filled');
    title(ax2, 'Diagrama G-G');
    xlabel(ax2, 'Lat (g)'); ylabel(ax2, 'Long (g)');
    grid(ax2, 'on'); axis(ax2, 'equal'); xlim(ax2, [-2 2]); ylim(ax2, [-2 2]);
    colorbar(ax2); colormap(ax2, 'jet');
    
    % MAPAS DA PISTA
    track_s = [0; cumsum(sqrt(diff(track_geom.X).^2 + diff(track_geom.Y).^2))];
    sim_pos_idx = interp1(track_s, 1:length(track_s), history.distance, 'nearest', 'extrap');
    sim_X = track_geom.X(sim_pos_idx);
    sim_Y = track_geom.Y(sim_pos_idx);
    
    % Mapa Velocidade
    ax3 = subplot(2,2,3);
    hold(ax3, 'on');
    plot(ax3, track_geom.X, track_geom.Y, 'Color', [0.8 0.8 0.8]);
    scatter(ax3, sim_X, sim_Y, 35, history.velocity*3.6, 'filled');
    title(ax3, 'Mapa de Velocidade'); axis(ax3, 'equal'); grid(ax3, 'on');
    colormap(ax3, 'hot'); colorbar(ax3);
    
    % Mapa Marcha
    ax4 = subplot(2,2,4);
    hold(ax4, 'on');
    plot(ax4, track_geom.X, track_geom.Y, 'Color', [0.8 0.8 0.8]);
    scatter(ax4, sim_X, sim_Y, 35, history.gear, 'filled');
    title(ax4, 'Mapa de Marchas'); axis(ax4, 'equal'); grid(ax4, 'on');
    colormap(ax4, parula(max(history.gear))); colorbar(ax4);
end