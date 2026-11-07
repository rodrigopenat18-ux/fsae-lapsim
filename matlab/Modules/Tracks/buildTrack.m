function [X, Y, Z, track_vectors] = buildTrack(segments)
    % buildTrack - Gera coordenadas 2D/3D de uma pista FSAE com inclinação.
    % ... (entradas/saídas) ...

    % --- Inicialização ---
    N = 150; 
    num_segments = size(segments, 1);
    total_points = num_segments * N;

    % Pré-alocação
    X = zeros(1, total_points);
    Y = zeros(1, total_points);
    Z = zeros(1, total_points);
    Angles = zeros(1, total_points);
    
    % %%% MUDANÇA: O vetor 'SlopeAngles_vec' foi removido,
    % %%% pois usaremos 'Z' para a cor.

    % Estado inicial
    current_pos = [0; 0; 0];
    current_angle = 0; 

    % --- Loop para construir a pista ---
    for i = 1:num_segments

        L = segments(i,2);
        R = segments(i,3);
        slope_angle = segments(i,4);
        type = segments(i,5);

        ds = L / N;                    
        dz = ds * tan(slope_angle);    

        for j = 1:N
            idx = (i-1)*N + j;

            % --- Trechos ---
            if type == 0   % RETA
                x_new = current_pos(1) + ds * cos(current_angle);
                y_new = current_pos(2) + ds * sin(current_angle);

            else           % CURVAS
                % ... (código das curvas) ...
                if type == 1   % CURVA ESQUERDA
                    cx = current_pos(1) - R * sin(current_angle);
                    cy = current_pos(2) + R * cos(current_angle);
                    current_angle = current_angle + (L / R)/N; % d_angle
                    x_new = cx + R * sin(current_angle);
                    y_new = cy - R * cos(current_angle);

                elseif type == 2   % CURVA DIREITA
                    cx = current_pos(1) + R * sin(current_angle);
                    cy = current_pos(2) - R * cos(current_angle);
                    current_angle = current_angle - (L / R)/N; % d_angle
                    x_new = cx - R * sin(current_angle);
                    y_new = cy + R * cos(current_angle);
                end
            end

            % Atualiza altitude
            z_new = current_pos(3) + dz;

            % Atualiza posição
            current_pos = [x_new; y_new; z_new];

            % Armazena
            X(idx) = x_new;
            Y(idx) = y_new;
            Z(idx) = z_new; % <-- Z (elevação) já é armazenada aqui
            Angles(idx) = current_angle;
            
            % %%% MUDANÇA: A linha 'SlopeAngles_vec(idx) = slope_angle;'
            % %%% foi removida.
        end
    end

    % --- Determinar se será 2D ou 3D ---
    hasSlope = any(abs(segments(:,4)) > 0.001);
    
    % --- Vetores de direção ---
    num_vectors = 50;
    vector_indices = round(linspace(1, length(X), num_vectors));
    U = cos(Angles(vector_indices));
    V = sin(Angles(vector_indices));
    track_vectors = [U', V'];

    
    % --- Bloco de plotagem ---
    
    figure; 

    if hasSlope
        % --- CASO 1: Pista com inclinação (Dois subplots) ---
        
        % --- Subplot 1: 3D com Elevação (Inalterado) ---
        ax1 = subplot(1, 2, 1); 
        hold(ax1, 'on');
        plot3(ax1, X, Y, Z, 'b-', 'LineWidth', 2);
        
        W = zeros(size(U)); 
        quiver3(ax1, X(vector_indices), Y(vector_indices), Z(vector_indices), ...
                U, V, W, 0.4, 'Color', [0 0.447 0.741], 'LineWidth', 1.5);
                
        xlabel(ax1, 'X (m)');
        ylabel(ax1, 'Y (m)');
        zlabel(ax1, 'Z (m) (Elevação)');
        title(ax1, 'Track Map em 3D (com Elevação)');
        view(ax1, 45, 25);
        axis(ax1, 'equal');
        grid(ax1, 'on');
        hold(ax1, 'off');

        % --- Subplot 2: 2D com Cor por Elevação (Z) ---
        ax2 = subplot(1, 2, 2); % Eixo direito
        hold(ax2, 'on');
        
        % %%% MUDANÇA AQUI: O 5º argumento agora é 'Z'
        scatter(ax2, X, Y, 10, Z, 'filled');
        
        xlabel(ax2, 'X (m)');
        ylabel(ax2, 'Y (m)');
        % %%% MUDANÇA AQUI: O título foi atualizado
        title(ax2, 'Track Map 2D (Cor = Elevação Z)');
        axis(ax2, 'equal');
        grid(ax2, 'on');
        
        % Adiciona a barra de cores
        cbar = colorbar(ax2);
        % %%% MUDANÇA AQUI: O label da barra foi atualizado
        ylabel(cbar, 'Elevação Z (m)'); 
        hold(ax2, 'off');
        
    else
        % --- CASO 2: Pista 2D (Sem inclinação) (Inalterado) ---
        ax1 = subplot(1, 1, 1);
        hold(ax1, 'on');
        plot(ax1, X, Y, 'b-', 'LineWidth', 2);
        
        quiver(ax1, X(vector_indices), Y(vector_indices), U, V, ...
               0.4, 'Color', [0 0.447 0.741], 'LineWidth', 1.5);
               
        xlabel(ax1, 'X (m)');
        ylabel(ax1, 'Y (m)');
        title(ax1, 'Track Map em 2D (sem inclinação)');
        axis(ax1, 'equal');
        grid(ax1, 'on');
        hold(ax1, 'off');
    end

    disp('Pista gerada usando ANGULO DE SLOPE (rad).');
end