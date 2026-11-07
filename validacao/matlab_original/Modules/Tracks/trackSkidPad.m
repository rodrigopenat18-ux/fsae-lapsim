function [segments] = trackSkidPad()
% Definição dos segmentos da pista
% 1 - Index
% 2 - Comprimento
% 3 - Raio
% 4 - Slope
% 5 - Type: 
%     - 0 = Straight
%     - 1 = Left
%     - 2 = Right 

segments = [
    1, 5.0, 0, 0, 0;
    2, 51.5, 8.2, 0, 1; 
];