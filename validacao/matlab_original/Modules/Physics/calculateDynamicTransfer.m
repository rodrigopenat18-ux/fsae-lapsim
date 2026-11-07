function [DeltaCarga_N] = calculateDynamicTransfer(car, AccLong_g, AccLat_g)
    % Calcula a MUDANÇA (delta) de carga em cada pneu devido à aceleração.
    % Ordem: [1:DE, 2:DD, 3:TD, 4:TE]
    
    DeltaCarga_N = zeros(4, 1); % Cria um vetor coluna
    
    % Converte acelerações para m/s^2
    AccLong_ms2 = AccLong_g * car.g;
    AccLat_ms2 = AccLat_g * car.g;
    
    % Calcula a transferência de carga em cada eixo
    delta_long = (car.cg_height / car.wheelbase) * car.mass * AccLong_ms2 / 2;
    delta_lat_diant = (car.cg_height / car.track_front_m) * car.mass * AccLat_ms2 / 2;
    delta_lat_tras = (car.cg_height / car.track_rear_m) * car.mass * AccLat_ms2 / 2;
    
    % Combina as transferências para cada pneu seguindo a convenção
    % (AccLat > 0 = curva para esquerda, pneu direito recebe carga)
    DeltaCarga_N(1) = -delta_long + delta_lat_diant; % DE
    DeltaCarga_N(2) = -delta_long - delta_lat_diant; % DD
    DeltaCarga_N(3) = +delta_long - delta_lat_tras;  % TD
    DeltaCarga_N(4) = +delta_long + delta_lat_tras;  % TE
end