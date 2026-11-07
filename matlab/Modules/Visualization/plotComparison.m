function plotComparison(simulation_history, comparison_history, trackType, comparison_label)
    % --- ETAPA 1: SINCRONIZAÇÃO (Mantida) ---
    velocity_threshold = 0.5; 
    sim_start_index = find(simulation_history.velocity > velocity_threshold, 1, 'first');
    if ~isempty(sim_start_index)
        sim_fields = fieldnames(simulation_history);
        for i = 1:length(sim_fields)
            if length(simulation_history.(sim_fields{i})) >= sim_start_index
                simulation_history.(sim_fields{i}) = simulation_history.(sim_fields{i})(sim_start_index:end);
            end
        end
        simulation_history.time = simulation_history.time - simulation_history.time(1);
    end

    % --- ETAPA 2: PREPARAÇÃO DO CONSUMO ---
    % Se o log de comparação não tiver combustível, cria vetor de zeros
    if ~isfield(comparison_history, 'fuel_consumed_g')
        comparison_history.fuel_consumed_g = zeros(size(comparison_history.time));
    end
    
    cutoff_time = max(simulation_history.time(end), comparison_history.time(end));

    % --- ETAPA 3: PLOTAGEM (6 GRÁFICOS) ---
    figure('Name', ['Comparativo: ' comparison_label], 'Position', [50, 50, 1000, 950]);

    % 1. Velocidade
    subplot(6,1,1);
    plot(simulation_history.time, simulation_history.velocity * 3.6, 'b', 'LineWidth', 2); hold on;
    plot(comparison_history.time, comparison_history.velocity * 3.6, 'r', 'LineWidth', 1.5);
    title('Velocidade (km/h)'); grid on; ylabel('km/h'); xlim([0, cutoff_time]);
    legend('Simulação', comparison_label);

    % 2. RPM
    subplot(6,1,2);
    plot(simulation_history.time, simulation_history.rpm, 'b', 'LineWidth', 2); hold on;
    plot(comparison_history.time, comparison_history.rpm, 'r', 'LineWidth', 1.5);
    title('RPM do Motor'); grid on; xlim([0, cutoff_time]);

    % 3. Aceleração Longitudinal (G)
    subplot(6,1,3);
    plot(simulation_history.time, simulation_history.acceleration / 9.81, 'b', 'LineWidth', 2); hold on;
    plot(comparison_history.time, comparison_history.acceleration / 9.81, 'r', 'LineWidth', 1.5);
    title('Aceleração Long. (G)'); grid on; xlim([0, cutoff_time]);

    % 4. Aceleração Lateral (G)
    subplot(6,1,4);
    plot(simulation_history.time, simulation_history.lateral_accel / 9.81, 'b', 'LineWidth', 2); hold on;
    plot(comparison_history.time, comparison_history.lateral_accel / 9.81, 'r', 'LineWidth', 1.5);
    title('Aceleração Lat. (G)'); grid on; xlim([0, cutoff_time]);

    % 5. CONSUMO ACUMULADO (NOVO)
    subplot(6,1,5);
    plot(simulation_history.time, simulation_history.fuel_consumed_g, 'b', 'LineWidth', 2); hold on;
    plot(comparison_history.time, comparison_history.fuel_consumed_g, 'r--', 'LineWidth', 1);
    title('Consumo Acumulado (g)'); grid on; ylabel('Gramas'); xlim([0, cutoff_time]);

    % 6. Marcha
    subplot(6,1,6);
    stairs(simulation_history.time, simulation_history.gear, 'b', 'LineWidth', 2); hold on;
    stairs(comparison_history.time, comparison_history.gear, 'r', 'LineWidth', 1.5);
    title('Marcha'); grid on; xlabel('Tempo (s)'); xlim([0, cutoff_time]);
    ylim([0, 7]);
end