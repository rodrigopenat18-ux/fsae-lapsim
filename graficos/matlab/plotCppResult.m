function SimOut = plotCppResult(result_dir, which_lap)
%PLOTCPPRESULT Abre os gráficos originais do MATLAB (plotResults) para um resultado do C++.
%
%   plotCppResult('resultados/autox')      % como o runSimulation: só a última volta
%   plotCppResult('resultados/autox', 1)   % volta 1
%   plotCppResult('resultados/autox', 0)   % todas as voltas juntas
%
%   Antes, gere o resultado com:  fsae_sim run --track autox --out resultados/autox

    if nargin < 2
        which_lap = [];
    end

    here = fileparts(mfilename('fullpath'));
    repo = fileparts(fileparts(here));
    addpath(here);
    addpath(fullfile(repo, 'validacao', 'matlab_original', 'Modules', 'Visualization'));

    SimOut = loadCppResult(result_dir);
    n_laps = numel(SimOut.Laps);
    if isempty(which_lap)
        which_lap = n_laps;   % runSimulation plota só a volta lançada (a última)
    end

    if which_lap == 0
        history = SimOut.History;
        label = 'todas as voltas';
    else
        if which_lap < 1 || which_lap > n_laps
            error('plotCppResult:lap', 'Volta %d não existe (a simulação tem %d).', which_lap, n_laps);
        end
        history = SimOut.Laps{which_lap};
        label = sprintf('volta %d de %d', which_lap, n_laps);
    end

    fprintf('%s (%s): tempo total %.3f s', SimOut.Summary.track.name, label, SimOut.Results.TotalTime);
    if n_laps > 1
        fprintf(' | voltas:');
        fprintf(' %.3f', SimOut.Summary.results.lap_times_s);
    end
    fprintf('\n');

    plotResults(SimOut.Track, history, SimOut.Car);
end
