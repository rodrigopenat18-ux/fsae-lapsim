function [cornersVelocity, segmentSlope, TrackLength] = initializeTrack(car, segments)
    [numberSegments, ~] = size(segments);
    cornersVelocity = NaN(numberSegments, 1);
    TrackLength = segments(:, 2);
    segmentSlope = zeros(numberSegments, 1);
    
    for k = 1:numberSegments
        segmentSlope(k) = segments(k, 4);
        cornerRadius = segments(k, 3);
        
        if cornerRadius == 0
            cornersVelocity(k) = inf;
            continue;
        end
        
        % Ângulo do slope
        theta = atan(segmentSlope(k)/100);
        
        % Passamos o índice k para identificar a curva no print
        cornersVelocity(k) = calculateMaxSafeCurveSpeed(car, cornerRadius, theta, k);
    end
end