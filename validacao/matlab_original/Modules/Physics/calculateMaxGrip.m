function [maxGripForce_N] = calculateMaxGrip(car, engineForce_N, normalForceOnDrivenWheels_N)
    % Esta função calcula a força de aderência máxima, considerando a
    % transição de atrito estático para cinético (patinagem).

    % 1. Calcula a aderência máxima teórica com base no atrito estático.
    %    A variável 'car.mux_static' é lida diretamente do objeto 'car'
    %    que foi inicializado pelo initializeCar.m.
    maxStaticGrip_N = car.mux_static * normalForceOnDrivenWheels_N;

    % 2. Verifica se a força do motor supera a aderência estática.
    if engineForce_N > maxStaticGrip_N
        % SE SIM: O pneu está patinando. A força de aderência cai para o valor cinético.
        maxGripForce_N = car.mux_kinetic * normalForceOnDrivenWheels_N;
    else
        % SE NÃO: O pneu não está patinando. A força de aderência é a estática.
        % (Neste caso, a força que acelera o carro será a do motor, que é menor).
        maxGripForce_N = maxStaticGrip_N;
    end
end