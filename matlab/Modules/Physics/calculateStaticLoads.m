    function [CargaEst_N] = calculateStaticLoads(car, slope)
    % Calcula a força normal em cada pneu com o carro parado.
    % Ordem: [1:DE, 2:DD, 3:TD, 4:TE]
    
    CargaEst_N = zeros(4, 1); % Cria um vetor coluna

    % Força peso perpendicular a pista
    perpendicular_weight_N = car.mass * car.g * cos(slope);
    
    carga_dianteira_por_pneu = (1 - car.weight_dist_rear_percent/100) * perpendicular_weight_N / 2;
    carga_traseira_por_pneu  = (car.weight_dist_rear_percent/100) * perpendicular_weight_N / 2;
    
    CargaEst_N(1) = carga_dianteira_por_pneu; % DE
    CargaEst_N(2) = carga_dianteira_por_pneu; % DD
    CargaEst_N(3) = carga_traseira_por_pneu;  % TD
    CargaEst_N(4) = carga_traseira_por_pneu;  % TE
end