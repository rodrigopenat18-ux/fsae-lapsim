function [position, velocity, acceleration] = carAccelerate(car, engineForce, drag, rollingResistance, slopeForce, initialVelocity, initialPosition, dt, slope, downforce)
    
    accel_guess = 0;
    accel_new = 5;
    
    iter_count = 0;
    max_iters = 100;
    while abs(accel_guess - accel_new) > 0.001 && iter_count < max_iters
        
        accel_guess = accel_new;
        
        % --- CORREÇÃO AQUI ---
        % Chamamos a calculateLoadTransfer passando a velocidade atual (initialVelocity)
        % Ela já retorna o Normal_N COM o Downforce embutido.
        [normalForces_N, ~] = calculateLoadTransfer(car, accel_guess / car.g, 0, slope, initialVelocity);
        
        % A força normal total nas rodas traseiras (pneus 3 e 4) já inclui a aero
        totalNormalForceRear = normalForces_N(3) + normalForces_N(4);
        
        % Calcula o limite de tração
        maxTractionForce = calculateMaxGrip(car, engineForce, totalNormalForceRear);
        
        % A força real é o menor valor entre o que o motor oferece e o pneu aguenta
        actualTractionForce = min(engineForce, maxTractionForce);
        
        % Calcula a nova aceleração (F = m.a)
        netForce = actualTractionForce - drag - rollingResistance - slopeForce;
        accel_new = netForce / car.mass;
        
        iter_count = iter_count + 1;
    end
    
    acceleration = accel_new;
    velocity = initialVelocity + acceleration * dt;
    position = initialPosition + initialVelocity * dt + (0.5 * acceleration * dt^2);
end