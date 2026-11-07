% Arquivo: setup_paths.m
% Função: Configura os caminhos relativos para todo o projeto FSAE

% 1. Pega o caminho onde ESTE arquivo (setup_paths.m) está salvo
currentScriptPath = fileparts(mfilename('fullpath'));

% 2. Define a Raiz do Projeto como sendo a pasta PAI desta pasta (Sobe um nível)
%    Isso faz com que a raiz seja "MATLAB - TESTES V9COMPLETO"
projectRoot = fileparts(currentScriptPath); 

% 3. Adiciona recursivamente tudo que está na raiz do projeto
addpath(genpath(projectRoot));

% 4. (Opcional) Remove pastas de backup/arquivo para evitar conflito
if isfolder(fullfile(projectRoot, '_ARCHIVE'))
    rmpath(genpath(fullfile(projectRoot, '_ARCHIVE')));
end
if isfolder(fullfile(projectRoot, '_OLD'))
    rmpath(genpath(fullfile(projectRoot, '_OLD')));
end

disp('--------------------------------------------------');
disp([' Projeto Configurado!']);
disp([' Raiz do Projeto detectada em: ' projectRoot]);
disp(' Pastas Modules, Data e Visualization adicionadas ao Path.');
disp('--------------------------------------------------');