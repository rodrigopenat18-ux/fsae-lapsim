function compareCppResult(result_dir, log_file, kind)
%COMPARECPPRESULT Compara um resultado do C++ com um log real ou do OptimumLap
%usando o plotComparison.m original.
%
%   compareCppResult('Results/autox', 'Data/Private/log_autox.csv')             % log real
%   compareCppResult('Results/autox', 'Data/Private/Pista_AutoX_OPLap.csv', 'oplap')
%
%   kind: 'log' (padrão) ou 'oplap'. Como no runSimulation, a simulação inteira
%   (todas as voltas) é comparada.

    if nargin < 3 || isempty(kind)
        kind = 'log';
    end
    here = fileparts(mfilename('fullpath'));
    repo = fileparts(fileparts(here));
    addpath(here);
    addpath(fullfile(repo, 'MATLAB', 'Original', 'Modules', 'Visualization'));
    addpath(fullfile(repo, 'MATLAB', 'Original', 'Modules', 'IO_Tools'));

    SimOut = loadCppResult(result_dir);
    switch lower(kind)
        case 'log'
            [other, ok] = loadRealLogFile(log_file);
            label = 'Log Real';
        case 'oplap'
            [other, ok] = loadOPLapLog(log_file);
            label = 'OPLap Log';
        otherwise
            error('compareCppResult:kind', 'kind deve ser ''log'' ou ''oplap''.');
    end
    if ~ok
        error('compareCppResult:load', 'Não foi possível ler %s', log_file);
    end
    track_type = 1;   % só usado pelo plotComparison para rótulos
    plotComparison(SimOut.History, other, track_type, label);
end
