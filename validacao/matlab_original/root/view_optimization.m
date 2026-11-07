%% Script: view_optimization.m
% Objetivo: Carregar um arquivo .mat gerado pelo modeFRONTIER e visualizar
clear; clc; close all;
setup_paths;

% 1. Abre uma janela para você escolher qual arquivo quer ver
[file, path] = uigetfile('*.mat', 'Selecione o arquivo de resultado (Optimization_Result.mat)');

if isequal(file, 0)
    disp('Nenhum arquivo selecionado.');
    return;
end

% 2. Carrega o arquivo
fprintf('Carregando %s ...\n', file);
load(fullfile(path, file), 'SimOut');

% 3. Exibe resumo no Command Window
disp('------------------------------------------------');
disp(' RESUMO DA SIMULAÇÃO CARREGADA');
disp('------------------------------------------------');

% Exibe tempos parciais se existirem
if isfield(SimOut.Results, 'Lap1_Time')
    fprintf('Volta 1 (Partida): %.3f s\n', SimOut.Results.Lap1_Time);
end

if isfield(SimOut.Results, 'Lap2_Time') && ~isnan(SimOut.Results.Lap2_Time)
    fprintf('Volta 2 (Lançada): %.3f s\n', SimOut.Results.Lap2_Time);
end

% Exibe a melhor volta (FastestLap) se existir, senão usa TotalTime
if isfield(SimOut.Results, 'FastestLap')
    fprintf('>> MELHOR VOLTA:   %.3f s\n', SimOut.Results.FastestLap);
end

% Fallback para o tempo total (antigo LapTimeTotal)
if isfield(SimOut.Results, 'TotalTime')
    fprintf('Tempo Total:       %.3f s\n', SimOut.Results.TotalTime);
elseif isfield(SimOut.Results, 'LapTimeTotal') % Suporte a arquivos antigos
    fprintf('Tempo Total:       %.3f s\n', SimOut.Results.LapTimeTotal);
end

fprintf('Velocidade Máx:    %.1f km/h\n', SimOut.Results.MaxVelocity);
fprintf('G Lateral Máx:     %.2f g\n', SimOut.Results.MaxLatG);
disp('------------------------------------------------');

% 4. Pergunta o que você quer ver
choice = listdlg('PromptString',{'O que deseja visualizar?'},...
    'SelectionMode','single',...
    'ListString',{'Dashboard Completo', 'Track Map Interativo', 'Ambos', 'Apenas Gráfico Padrão'});

% 5. Chama suas funções antigas usando os dados salvos
switch choice
    case 1 % Dashboard
        try
            plotDashboard(SimOut.History, SimOut.Car); 
        catch
            plotDashboard(SimOut.History);
        end
        
    case 2 % Interactive Track
        plotInteractiveTrack(SimOut.Track, SimOut.History, SimOut.Car);
        
    case 3 % Ambos
        try
            plotDashboard(SimOut.History, SimOut.Car);
        catch
            plotDashboard(SimOut.History);
        end
        
        figure; 
        plotInteractiveTrack(SimOut.Track, SimOut.History, SimOut.Car);
        
    case 4 % Plot Results Padrão
        plotResults(SimOut.Track, SimOut.History, SimOut.Car);
end