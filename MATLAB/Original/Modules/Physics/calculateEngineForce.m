function [engine_Force] = calculateEngineForce(car, final_EngineTorque)

% considerando dois pneus tracionando

engine_Force = (final_EngineTorque) / car.tire_radius;

end
