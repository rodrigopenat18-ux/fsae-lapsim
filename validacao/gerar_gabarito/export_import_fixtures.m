function export_import_fixtures(out_dir)
%EXPORT_IMPORT_FIXTURES Gera planilhas de teste para a importação de curvas.
%   matlab -batch "addpath('validacao/gerar_gabarito'); export_import_fixtures"
%
%   validacao/gabarito/import/engine.xlsx  (escrito pelo MATLAB, formato .xlsx real)
%     planilha "Torque": linha de título, cabeçalho e as curvas do dataEngine.m
%     planilha "Marchas": relações e rpm de subida do initializeCar.m

    here = fileparts(mfilename('fullpath'));
    repo = fileparts(fileparts(here));
    if nargin < 1
        out_dir = fullfile(repo, 'validacao', 'gabarito', 'import');
    end
    if ~isfolder(out_dir), mkdir(out_dir); end
    addpath(genpath(fullfile(repo, 'validacao', 'matlab_original')));

    car = initializeCar();
    rpm = car.TorqueInterpol.GridVectors{1}(:);
    torque = car.TorqueInterpol.Values(:);

    f = fullfile(out_dir, 'engine.xlsx');
    if isfile(f), delete(f); end
    C = [{'Curva de torque - motor (dataEngine.m)', '', ''}; ...
         {'RPM', 'Torque [N.m]', 'Observação'}; ...
         [num2cell(rpm), num2cell(torque), repmat({'medido'}, numel(rpm), 1)]];
    writecell(C, f, 'Sheet', 'Torque');
    G = [{'Marcha', 'Relação', 'RPM de subida'}; ...
         num2cell([(1:numel(car.gear_ratios))', car.gear_ratios(:), car.shiftRPM_vec(:)])];
    writecell(G, f, 'Sheet', 'Marchas');

    fprintf('Gerado %s (%d pontos de torque, %d marchas)\n', f, numel(rpm), numel(car.gear_ratios));
end
