function SimOut = loadCppResult(result_dir)
%LOADCPPRESULT Lê a pasta gravada por "fsae_sim run" no formato do SimOut do MATLAB.
%
%   SimOut = loadCppResult('results/autox')
%
%   Campos devolvidos (mesmos nomes do runSimulation.m original):
%     SimOut.History   struct de vetores-linha: time, distance, velocity, gear,
%                      acceleration, rpm, lateral_accel, drag, downforce,
%                      rolling_resistance, slope_force, engine_force, brake_force,
%                      brake_temp, h_coef, fuel_consumed_g, P_conv, P_rad, P_cond,
%                      load_front_dist e lap (número da volta de cada amostra)
%     SimOut.Track     struct com X, Y, Z (traçado do buildTrack)
%     SimOut.Results   TotalTime, MaxVelocity, MaxLatG, MaxBrakeTemp, TotalFuel_g,
%                      Lap1_Time, Lap2_Time, FastestLap
%     SimOut.Car       campos usados pelos gráficos: gear_ratios, shiftRPM_vec,
%                      rpm_min, rpm_max, launchRPM, downshiftRPM, mass
%     SimOut.Laps      cell array com o histórico de cada volta, recortado como o
%                      runSimulation faz para plotar a volta 2 (tempo, distância e
%                      combustível começando do zero)
%     SimOut.Summary   summary.json completo (inclui tempos e consumo por volta)
%
%   Não depende de nenhuma toolbox.

    if nargin < 1
        error('loadCppResult:args', 'Informe a pasta de resultados (ex.: ''results/autox'').');
    end
    must_exist(result_dir, 'telemetry.csv');
    must_exist(result_dir, 'summary.json');

    % --- Histórico ---
    T = readmatrix(fullfile(result_dir, 'telemetry.csv'), 'FileType', 'text', ...
                   'NumHeaderLines', 1, 'Delimiter', ',');
    header = strsplit(strtrim(fileread_firstline(fullfile(result_dir, 'telemetry.csv'))), ',');
    if size(T, 2) ~= numel(header)
        error('loadCppResult:format', 'telemetry.csv com %d colunas, cabeçalho com %d.', ...
              size(T, 2), numel(header));
    end
    History = struct();
    for k = 1:numel(header)
        History.(header{k}) = T(:, k).';   % vetores-linha, como no MATLAB original
    end
    SimOut.History = History;

    % --- Resumo ---
    S = jsondecode(fileread(fullfile(result_dir, 'summary.json')));
    SimOut.Summary = S;
    SimOut.Results = S.matlab_results;
    SimOut.Results = null_to_nan(SimOut.Results);

    % --- Traçado ---
    geo_file = fullfile(result_dir, 'geometry.csv');
    if isfile(geo_file)
        G = readmatrix(geo_file, 'FileType', 'text', 'NumHeaderLines', 1, 'Delimiter', ',');
        SimOut.Track = struct('X', G(:, 1).', 'Y', G(:, 2).', 'Z', G(:, 3).');
    else
        SimOut.Track = struct('X', [], 'Y', [], 'Z', []);
    end

    % --- Carro (só o que os gráficos usam) ---
    car_file = fullfile(result_dir, 'car.json');
    if isfile(car_file)
        C = jsondecode(fileread(car_file));
        SimOut.Car = struct( ...
            'name', C.name, ...
            'mass', C.mass.car_kg + C.mass.driver_kg, ...
            'gear_ratios', C.powertrain.gear_ratios(:).', ...
            'shiftRPM_vec', C.powertrain.upshift_rpm(:).', ...
            'launchRPM', C.powertrain.launch_rpm, ...
            'downshiftRPM', C.powertrain.downshift_rpm, ...
            'rpm_min', min(C.engine.rpm), ...
            'rpm_max', max(C.engine.rpm));
    else
        SimOut.Car = struct();
    end

    % --- Voltas ---
    laps = unique(History.lap);
    SimOut.Laps = cell(1, numel(laps));
    names = fieldnames(History);
    for k = 1:numel(laps)
        idx = History.lap == laps(k);
        L = struct();
        for f = 1:numel(names)
            L.(names{f}) = History.(names{f})(idx);
        end
        if k > 1
            L.time = L.time - L.time(1);
            L.distance = L.distance - (k - 1) * S.track.length_m;
            L.fuel_consumed_g = L.fuel_consumed_g - L.fuel_consumed_g(1);
        end
        SimOut.Laps{k} = L;
    end
end

function must_exist(d, f)
    if ~isfile(fullfile(d, f))
        error('loadCppResult:missing', 'Arquivo não encontrado: %s', fullfile(d, f));
    end
end

function line = fileread_firstline(path)
    fid = fopen(path, 'r');
    c = onCleanup(@() fclose(fid));
    line = fgetl(fid);
end

function R = null_to_nan(R)
    % jsondecode transforma null em [] — no MATLAB original era NaN.
    f = fieldnames(R);
    for k = 1:numel(f)
        if isempty(R.(f{k})), R.(f{k}) = NaN; end
    end
end
