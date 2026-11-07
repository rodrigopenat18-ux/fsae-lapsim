%% =========================================================================
%                INICIALIZAÇÃO E ARQUIVO GPX
% =========================================================================
clc;
clear;
close all;
% Definir o nome do arquivo GPX uma vez
gpxFile = 'FSAEB_2023_Drivers_Walk (1).gpx';

%% =========================================================================
%   PARTE 1: CÁLCULO DA MATRIZ (COM CONTINUIDADE E LOOP-CLOSURE)
% =========================================================================
disp("=================================================");
disp("===== PARTE 1: MATRIZ FINAL (Slope com Fechamento de Loop) =====");
disp("=================================================");
% 1) Carregar sua matriz de segmentos
%    (Carregando a versão com 50 segmentos)
segments_in = track_AutoX_2024_V1_original(); 

% 2) Calcular slopes usando a NOVA FUNÇÃO (applyAltitudeFromGPX)
segments_out = applyAltitudeFromGPX(segments_in, gpxFile);

disp("------ MATRIZ FINAL (segments_out) ------");
disp(segments_out);

%% =========================================================================
%   PARTE 2: PLOTS COMPARATIVOS
% =========================================================================
fprintf('\n\n'); % Adiciona espaço no console
disp("=================================================");
disp("===== PARTE 2: GERANDO PLOTS COMPARATIVOS =====");
disp("=================================================");
% 1) LEITURA DO ARQUIVO GPX (para os plots)
xml = xmlread(gpxFile);
trkpts = xml.getElementsByTagName('trkpt');
n = trkpts.getLength;
lat = zeros(n,1);
lon = zeros(n,1);
ele = zeros(n,1); % Dados de altitude "crus" (com ruído)
for i = 1:n
    item = trkpts.item(i-1);
    lat(i) = str2double(item.getAttribute('lat'));
    lon(i) = str2double(item.getAttribute('lon'));
    eleNode = item.getElementsByTagName('ele').item(0);
    ele(i) = str2double(eleNode.getTextContent);
end
% 2) Suavizar dados de altitude (para o plot melhorado)
windowSize = 5; 
ele_suavizado = movmean(ele, windowSize);
%% =============================================
%   PLOT 1: MÉTODO ORIGINAL (MÉDIA POR SEGMENTO)
% =============================================
figure(1); % Abre a Figura 1
hold on; grid on; axis equal;
title('Plot 1: Original (Média por Segmento)');
xlabel('Longitude'); ylabel('Latitude');
% 2.1) DEFINIR TRECHOS POR ALTITUDE (Lógica original)
limiar = 0.5;   
segment_id = 1;
segments_plot = zeros(n,1); 
segments_plot(1) = segment_id;
for i = 2:n
    if abs(ele(i) - ele(i-1)) > limiar 
        segment_id = segment_id + 1;
    end
    segments_plot(i) = segment_id;
end
% 2.2) CALCULAR ALTITUDE MÉDIA POR TRECHO (Lógica original)
unique_segments = unique(segments_plot);
num_seg = length(unique_segments);
avg_alt_plot = zeros(num_seg,1); 
for s = 1:num_seg
    avg_alt_plot(s) = mean(ele(segments_plot == unique_segments(s)));
end
% 2.3) PLOTAR (Lógica original)
cmap = jet(num_seg);
for s = 1:num_seg
    idx = (segments_plot == unique_segments(s));
    plot(lon(idx), lat(idx), 'LineWidth', 3, 'Color', cmap(s,:));
end
cb = colorbar;
colormap(jet(num_seg));
caxis([min(avg_alt_plot) max(avg_alt_plot)]);
ylabel(cb, 'Altitude Média (m)');
% 2.4) DATA CURSOR (Mostra altitude CRUA 'ele')
dcm_obj1 = datacursormode(gcf); 
set(dcm_obj1, 'UpdateFcn', {@myDataTip, lon, lat, ele}); 
disp("Figura 1 (Original) gerada.");
%% =============================================
%   PLOT 2: MÉTODO MELHORADO (PONTUAL COM SCATTER)
% =============================================
figure(2); % Abre a Figura 2
hold on; grid on; axis equal;
title('Plot 2: Melhorado (Ponto a Ponto Suavizado)');
xlabel('Longitude'); ylabel('Latitude');
% 3.1) PLOTAR (Lógica melhorada)
scatter(lon, lat, 15, ele_suavizado, 'filled'); 
cb = colorbar;
colormap(jet); 
caxis([min(ele_suavizado) max(ele_suavizado)]); 
ylabel(cb, 'Altitude Pontual Suavizada (m)');
% 3.2) DATA CURSOR (Mostra altitude SUAVIZADA 'ele_suavizado')
dcm_obj2 = datacursormode(gcf);
set(dcm_obj2, 'UpdateFcn', {@myDataTip, lon, lat, ele_suavizado}); 
disp("Figura 2 (Melhorada) gerada.");

%% =========================================================================
%                     FUNÇÕES AUXILIARES
% =========================================================================

% =============================================
%   FUNÇÃO: APLICAR ALTITUDE (VERSÃO FINAL - CONTÍNUA + ERRO ZERO)
% =============================================
function segments_out = applyAltitudeFromGPX(segments, gpx_file)
    % 1) Ler dados do GPX
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
    
    % Suavizar o ruído da altitude
    windowSize = 5; 
    ele_suavizado = movmean(ele, windowSize);
    
    % 2) Converter lat/lon para distância cumulativa (dist_real)
    lat0 = lat(1);
    lon0 = lon(1);
    R = 6371000; % raio da Terra (m)
    x = R * (deg2rad(lon - lon0)) .* cosd(lat0);
    y = R * (deg2rad(lat - lat0));
    dx = diff(x);
    dy = diff(y);
    dist_real = [0; cumsum(sqrt(dx.^2 + dy.^2))]; % Vetor de distância do GPX
    
    % ------------------------------------------------------------
    % 3) %% LÓGICA NOVA: FORÇAR CONTINUIDADE %%
    % ------------------------------------------------------------
    
    seg_len     = segments(:,2);
    L_total_track = sum(seg_len);
    
    % Criar vetor de "nós" (junções) da pista.
    junction_dists = [0; cumsum(seg_len)];
    
    % ==========================================================
    % %% CORREÇÃO DO ERRO (INÍCIO) %%
    % Remove pontos de distância duplicados (GPS parado)
    [unique_dist_real, idx_unique] = unique(dist_real);
    % Filtra o vetor de elevação para manter alinhado
    unique_ele_suavizado = ele_suavizado(idx_unique);
    % %% CORREÇÃO DO ERRO (FIM) %%
    % ==========================================================
    
    % Interpolar a altitude do GPX (dist_real, ele_suavizado)
    % nos locais exatos dos nossos "nós" (junction_dists)
    junction_alts = interp1(unique_dist_real, unique_ele_suavizado, junction_dists, 'linear', 'extrap');

    % Agora, AltInicial e AltFinal são contínuos por definição
    alt_inicial_continuo = junction_alts(1:end-1);
    alt_final_continuo = junction_alts(2:end);
    
    % Calcular o Delta_Z de cada segmento
    delta_z_segmento = alt_final_continuo - alt_inicial_continuo;
    
    % ------------------------------------------------------------
    % 4) %% LÓGICA NOVA: FORÇAR FECHAMENTO DE LOOP (ERRO ZERO) %%
    % ------------------------------------------------------------
    
    % Calcular o erro total do GPX (quanto ele "caiu" do início ao fim)
    z_error_total = junction_alts(end) - junction_alts(1);
    
    % Distribuir esse erro por todos os segmentos, proporcional ao comprimento
    z_correction = - (z_error_total * (seg_len / L_total_track));
    
    % Aplicar a correção ao Delta_Z de cada segmento
    delta_z_corrigido = delta_z_segmento + z_correction;
    
    % ------------------------------------------------------------
    % 5) Calcular o ÂNGULO (Slope) final
    % ------------------------------------------------------------
    slope_angle_rad = atan(delta_z_corrigido ./ seg_len);
    
    % ------------------------------------------------------------
    % 6) Atualizar matriz com o ÂNGULO
    % ------------------------------------------------------------
    segments_out = segments;
    segments_out(:,4) = slope_angle_rad; % <- Coloca o ângulo na 4ª coluna
    
    % ------------------------------------------------------------
    % 7) Tabela para visualização (atualizada)
    % ------------------------------------------------------------
    disp("------ Slopes (Ângulos) Corrigidos e Contínuos ------")
    fprintf('Erro total do GPX (distribuído): %.4f m\n', z_error_total);
    T = table((1:length(slope_angle_rad))', seg_len, alt_inicial_continuo, alt_final_continuo, delta_z_corrigido, slope_angle_rad, ...
        'VariableNames', {'Segmento','Comprimento(L)','AltInicial','AltFinal','Delta_Z_Corrigido','Slope(rad)'});
    disp(T)
end

% =============================================
%   FUNÇÃO: MATRIZ ORIGINAL DA PISTA (Versão com 52 segmentos)
% =============================================
function [segments] = track_AutoX_2024_V1_original()
% %% MUDANÇA %%
% O Segmento 32 foi dividido em quatro (32, 33, 34, 35).
% Todos os segmentos seguintes foram re-indexados (agora 52 no total).
segments = [
    1, 34.0737, 0,        0, 0;
    2, 14.5054, 13.5392,  0, 2;
    3, 26.4637, 0,        0, 0;
    4, 5.4370, 6,         0, 2;
    5, 9.1336, 6,         0, 1;
    6, 4.1719, 6,         0, 2;
    7, 26.7556, 0,        0, 0;
    8, 38.8195, 11.9248,  0, 2;
    9, 37.6596, 0,        0, 0;
    10,21.2035,20.3746,   0, 1;
    11,30.4424,0,         0, 0;
    12,5.0506, 7.5,       0, 1;
    13,7.9527, 7.5,       0, 2;
    14,6.5373, 7.5,       0, 1;
    15,3.2686, 7.5,       0, 2;
    16,6.6097, 0,         0, 0;
    17,21.7863,92.9899,   0, 1;
    18,28.9214,11.1392,   0, 1;
    19,8.0988, 5.9883,    0, 2;
    20,15.8682,12.3759,   0, 1;
    21,57.6727,0,         0, 0;
    22,11.5828,5.6292,    0, 1;
    23,16.5647,0,         0, 0;
    24,17.0366,7.6445,    0, 1;
    25,11.3541,8.1833,    0, 2;
    26,14.2013,0,         0, 0;
    27,18.4848,7.7655,    0, 2;
    28,24.6527,0,         0, 0;
    29,5.3048, 6.5,       0, 2;
    30,6.0340, 6.5,       0, 1;
    31,10.4416,6.5,       0, 2;
    32, 11.483125, 0,       0, 0; % Quarto 1 do antigo Seg 32
    33, 11.483125, 0,       0, 0; % Quarto 2 do antigo Seg 32
    34, 11.483125, 0,       0, 0; % Quarto 3 do antigo Seg 32
    35, 11.483125, 0,       0, 0; % Quarto 4 do antigo Seg 32
    36, 24.5161,32.0785,   0, 1; % Antigo Seg 33
    37, 8.0978, 0,         0, 0; % Antigo Seg 34
    38, 39.8798,9.6967,    0, 2; % Antigo Seg 35
    39, 24.8055,11.7005,   0, 1; % Antigo Seg 36
    40, 13.5000,12.3735,   0, 2; % Antigo Seg 37
    41, 33.1894,0,         0, 0; % Antigo Seg 38
    42, 6.2701, 6,         0, 1; % Antigo Seg 39
    43, 11.3427,6,         0, 2; % Antigo Seg 40
    44, 11.5372,6,         0, 1; % Antigo Seg 41
    45, 11.1947,6,         0, 2; % Antigo Seg 42
    46, 11.2506,6,         0, 1; % Antigo Seg 43
    47, 6.2549, 6,         0, 2; % Antigo Seg 44
    48, 21.0002,0,         0, 0; % Antigo Seg 45
    49, 32.7298,22.7008,   0, 2; % Antigo Seg 46
    50, 17.0000,0,         0, 0; % Antigo Seg 47
    51, 28.0000,26.9275,   0, 2; % Antigo Seg 48
    52, 61.6830,0,         0, 0; % Antigo Seg 49
    % --- FIM DA MUDANÇA ---
];
end

function txt = myDataTip(~, event_obj, all_lon, all_lat, all_ele_data)
    % Obtém posição do cursor
    pos = event_obj.Position; 
    
    % Calcula índice do ponto mais próximo
    dist_sq = (all_lon - pos(1)).^2 + (all_lat - pos(2)).^2;
    [~, idx] = min(dist_sq);
    
    % Prepara texto do data tip
    txt = { ...
        ['Longitude: ', num2str(all_lon(idx),'%.6f')], ...
        ['Latitude: ',  num2str(all_lat(idx),'%.6f')], ...
        ['Altitude: ',  num2str(all_ele_data(idx),'%.2f'), ' m'] ...
    };
end
