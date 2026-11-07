function [position, velocity, acceleration, actualbrakeForce] = carBrake(car, brakeForce, drag, rollingResistance, slopeForce, initialVelocity, initialPosition, dt, slope, downforce)
    accel_guess = 0;
    accel_new = -5; 
    
    iter_count = 0;
    max_iters = 300;
    
    while abs(accel_guess - accel_new) > 0.001 && iter_count < max_iters
        
        accel_guess = accel_new;
        
        % --- CORREÇÃO 1: Passar a velocidade para calcular o Downforce interno ---
        [normalForces_N, ~] = calculateLoadTransfer(car, accel_guess / car.g, 0, slope, initialVelocity);
        
        % --- CORREÇÃO 2: A carga total já contém o Downforce calculado lá dentro ---
        totalNormalForce = sum(normalForces_N); 
        
        % Limite de aderência longitudinal total
        maxBrakingGrip = car.mux_static * totalNormalForce;
        
        % A força de frenagem real não pode exceder o que o pneu aguenta (Grip)
        actualbrakeForce = min(brakeForce, maxBrakingGrip);
        
        % F = m.a -> Forças de resistência ajudam a frear (sinal negativo)
        netForce = -(actualbrakeForce + drag + rollingResistance) - slopeForce;
        
        accel_new = netForce / car.mass;
        
        iter_count = iter_count + 1;
    end
    
    acceleration = accel_new;
    velocity = initialVelocity + acceleration * dt;
    
    if velocity < 0
        velocity = 0;
        % Opcional: ajustar aceleração média se o carro parou no meio do dt
    end
    
    position = initialPosition + initialVelocity * dt + (0.5 * acceleration * dt^2);
end