function export_gpx_reference(out_dir)
%EXPORT_GPX_REFERENCE Gera validacao/gabarito/gpx/ para validar o cálculo de slope por GPX.
%
%   O GPX real da equipe é privado, então este script cria um GPX SINTÉTICO
%   (traçado da autox_2024_v1 com uma elevação artificial e determinística) e
%   aplica a mesma lógica do applyAltitudeFromGPX (função local do script
%   matlab/Modules/Visualization/Plot_GPX.m, copiada abaixo sem alterações de
%   lógica, porque funções locais de script não podem ser chamadas de fora).
%
%   matlab -batch "addpath('validacao/gerar_gabarito'); export_gpx_reference"

    here = fileparts(mfilename('fullpath'));
    repo = fileparts(fileparts(here));
    if nargin < 1
        out_dir = fullfile(repo, 'validacao', 'gabarito', 'gpx');
    end
    if ~isfolder(out_dir), mkdir(out_dir); end
    addpath(genpath(fullfile(repo, 'validacao', 'matlab_original')));
    set(0, 'DefaultFigureVisible', 'off');

    % Segmentos da pista (slope zerado, como o track_AutoX_2024_V1_original do Plot_GPX)
    segments = readmatrix(fullfile(repo, 'validacao', 'gabarito', 'tracks', 'autox2024v1', 'segments.csv'));
    segments(:, 4) = 0;

    % --- GPX sintético ---
    [X, Y] = buildTrack(segments);
    close all force;
    X = [0, X]; Y = [0, Y];
    step = 6;                                    % um ponto a cada ~6 posições do traçado
    idx = [1:step:numel(X), numel(X)];
    x = X(idx); y = Y(idx);
    s = [0, cumsum(hypot(diff(x), diff(y)))];
    ele = 545 + 3 * sin(2*pi*s/400) + 0.8 * sin(s/23) + 0.002 * s;   % termina 1,9 m acima
    ele = round(ele * 10) / 10;                  % resolução de 0,1 m, como o Strava
    lat0 = -22.7377460; lon0 = -47.5339950; R = 6371000;
    lat = lat0 + rad2deg(y / R);
    lon = lon0 + rad2deg(x ./ (R * cosd(lat0)));
    % Pontos repetidos (o GPS parado gera distância repetida; testa o unique)
    rep = [40, 41, 200];
    order = sort([1:numel(lat), rep]);
    lat = lat(order); lon = lon(order); ele = ele(order);

    gpx_file = fullfile(out_dir, 'synthetic.gpx');
    fid = fopen(gpx_file, 'w');
    fprintf(fid, '<?xml version="1.0" encoding="UTF-8"?>\n');
    fprintf(fid, '<gpx creator="export_gpx_reference.m" version="1.1" xmlns="http://www.topografix.com/GPX/1/1">\n');
    fprintf(fid, ' <trk>\n  <name>Sintetico - autox_2024_v1</name>\n  <trkseg>\n');
    for k = 1:numel(lat)
        fprintf(fid, '   <trkpt lat="%.7f" lon="%.7f">\n    <ele>%.1f</ele>\n   </trkpt>\n', lat(k), lon(k), ele(k));
    end
    fprintf(fid, '  </trkseg>\n </trk>\n</gpx>\n');
    fclose(fid);

    % --- Referência (lógica do applyAltitudeFromGPX) ---
    [segments_out, T, z_err] = applyAltitudeFromGPX(segments, gpx_file);
    write_matrix(fullfile(out_dir, 'segments_in.csv'), 'index,length,radius,slope,type', segments);
    write_matrix(fullfile(out_dir, 'segments_out.csv'), 'index,length,radius,slope,type', segments_out);
    write_matrix(fullfile(out_dir, 'slopes.csv'), 'segment,length,alt_start,alt_end,delta_z,slope', T);
    fid = fopen(fullfile(out_dir, 'z_error.csv'), 'w');
    fprintf(fid, 'key,value\nz_error_total,%.17g\n', z_err);
    fclose(fid);
    fprintf('GPX sintético: %d pontos; erro de fechamento %.4f m\n', numel(lat), z_err);
end

function write_matrix(path, header, M)
    fid = fopen(path, 'w');
    fprintf(fid, '%s\n', header);
    fmt = [strjoin(repmat({'%.17g'}, 1, size(M, 2)), ','), '\n'];
    fprintf(fid, fmt, M');
    fclose(fid);
end

% ------------------------------------------------------------------------
% Cópia do applyAltitudeFromGPX (Plot_GPX.m). Mudanças: só a saída (devolve a
% tabela e o erro de fechamento em vez de imprimir).
% ------------------------------------------------------------------------
function [segments_out, T, z_error_total] = applyAltitudeFromGPX(segments, gpx_file)
    gpx = xmlread(gpx_file);
    trkpt = gpx.getElementsByTagName('trkpt');
    N = trkpt.getLength;
    lat = zeros(N,1);
    lon = zeros(N,1);
    ele = zeros(N,1);
    for i = 1:N
        node = trkpt.item(i-1);
        lat(i) = str2double(char(node.getAttribute('lat')));
        lon(i) = str2double(char(node.getAttribute('lon')));
        eleNode = node.getElementsByTagName('ele').item(0);
        ele(i) = str2double(eleNode.getTextContent);
    end
    windowSize = 5;
    ele_suavizado = movmean(ele, windowSize);
    lat0 = lat(1);
    lon0 = lon(1);
    R = 6371000;
    x = R * (deg2rad(lon - lon0)) .* cosd(lat0);
    y = R * (deg2rad(lat - lat0));
    dx = diff(x);
    dy = diff(y);
    dist_real = [0; cumsum(sqrt(dx.^2 + dy.^2))];
    seg_len     = segments(:,2);
    L_total_track = sum(seg_len);
    junction_dists = [0; cumsum(seg_len)];
    [unique_dist_real, idx_unique] = unique(dist_real);
    unique_ele_suavizado = ele_suavizado(idx_unique);
    junction_alts = interp1(unique_dist_real, unique_ele_suavizado, junction_dists, 'linear', 'extrap');
    alt_inicial_continuo = junction_alts(1:end-1);
    alt_final_continuo = junction_alts(2:end);
    delta_z_segmento = alt_final_continuo - alt_inicial_continuo;
    z_error_total = junction_alts(end) - junction_alts(1);
    z_correction = - (z_error_total * (seg_len / L_total_track));
    delta_z_corrigido = delta_z_segmento + z_correction;
    slope_angle_rad = atan(delta_z_corrigido ./ seg_len);
    segments_out = segments;
    segments_out(:,4) = slope_angle_rad;
    T = [(1:length(slope_angle_rad))', seg_len, alt_inicial_continuo, alt_final_continuo, ...
         delta_z_corrigido, slope_angle_rad];
end
