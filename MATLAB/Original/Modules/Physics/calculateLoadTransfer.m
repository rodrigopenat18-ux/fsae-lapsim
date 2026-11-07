
% Convenção na nomeclatura dos pneus:
%   - Numeração sequencial sentido horário, começando do DE
%
%    DE           DD
%    ___         ___
%   |   |       |   |
%   | 1 |-------| 2 |
%   |___|   |   |___|
%           |
%           |
%           |
%    ___    |    ___
%   |   |   |   |   |
%   | 4 |-------| 3 |
%   |___|       |___|
%    TE          TD
%
% Convenção na aceleração (mesma usada no diagrama g-g do Milliken):
%   - AccLong_ms > 0 = Aceleração
%   - AccLong_ms < 0 = Frenagem
%   - AccLat_ms2 > 0 = Curva à esquerda
%   - AccLat_ms2 < 0 = Curva à direita

function [Normal_N, DistrMassa_perc] = calculateLoadTransfer(car, AccLong_g, AccLat_g, slope, velocity)
    % 1. Cargas estáticas (Peso)
    CargasEstaticas_N = calculateStaticLoads(car, slope);
    
    % 2. Cargas dinâmicas (Transferência de massa)
    CargasTransferidas_N = calculateDynamicTransfer(car, AccLong_g, AccLat_g);
    
    % 3. Carga Aerodinâmica (Downforce)
    % Precisamos calcular o downforce e distribuir pelos pneus
    F_down = calculateDownforce(car, velocity);
    
    % Distribuímos o downforce entre frente e traseira (geralmente segue o centro de pressão)
    % Se não tiver dados de aero map, use a distribuição de peso como aproximação:
    dist_F = (1 - car.cpaero/100);
    dist_R = (car.cpaero/100);
    
    CargaAero_N = [dist_F; dist_F; dist_R; dist_R] * (F_down / 2);
    
    % 4. A carga REAL total que o pneu sente
    Normal_N = CargasEstaticas_N + CargasTransferidas_N + CargaAero_N;
    
    % 5. Agora a soma de Normal_N será (Peso + Downforce)
    % A divisão pelo sum(Normal_N) no runSimulation agora nunca passará de 100%
    DistrMassa_perc = (Normal_N / sum(Normal_N)) * 100;
end
