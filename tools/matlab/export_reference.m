function export_reference(out_dir)
%EXPORT_REFERENCE Gera os dados de referência ("gabarito") para validar o port C++.
%
%   export_reference()          grava em <repo>/tests/reference
%   export_reference(out_dir)   grava na pasta indicada
%
%   Executa o simulador MATLAB ORIGINAL (pasta matlab/) sem modificá-lo e salva,
%   em CSV com 17 algarismos significativos:
%     unit/   saídas de funções isoladas sobre grades de entradas
%     straight/  casos do calculate_Straight (histórico completo)
%     tracks/<pista>/  segmentos, geometria, velocidades de curva,
%                      histórico completo e resultados da simulação
%
%   Uso em modo batch (sem interface gráfica):
%     matlab -batch "addpath('<repo>/tools/matlab'); export_reference"

    here = fileparts(mfilename('fullpath'));
    repo = fileparts(fileparts(here));
    if nargin < 1 || isempty(out_dir)
        out_dir = fullfile(repo, 'tests', 'reference');
    end
    matlab_dir = fullfile(repo, 'matlab');
    addpath(genpath(matlab_dir));
    set(groot, 'DefaultFigureVisible', 'off');

    unit_dir = fullfile(out_dir, 'unit');
    straight_dir = fullfile(out_dir, 'straight');
    ensure_dir(unit_dir); ensure_dir(straight_dir);

    manifest = struct();
    manifest.matlab_version = version();
    manifest.computer = computer();
    manifest.generated_at = char(datetime('now', 'Format', 'yyyy-MM-dd HH:mm:ss'));
    manifest.timings_s = struct();

    car = initializeCar();

    fprintf('[0/10] Funcoes matematicas (linspace, interp1, pchip)\n');
    export_math(unit_dir);

    fprintf('[1/10] Parametros do carro e motor\n');
    export_car(car, unit_dir);

    fprintf('[2/10] Primitivas (aero, pneu, rampa)\n');
    export_primitives(car, unit_dir);

    fprintf('[3/10] Transferencia de carga\n');
    export_load_transfer(car, unit_dir);

    fprintf('[4/10] Powertrain\n');
    export_powertrain(car, unit_dir);

    fprintf('[5/10] Temperatura do freio\n');
    export_brake_temp(car, unit_dir);

    fprintf('[6/10] Cambio e forcas\n');
    export_gearbox(car, unit_dir);

    fprintf('[7/10] carAccelerate / carBrake\n');
    export_longitudinal(car, unit_dir);

    fprintf('[8/10] Velocidade maxima em curva\n');
    export_corner_speed(car, unit_dir);

    fprintf('[9/10] calculate_Straight\n');
    manifest.timings_s.straight_cases = export_straights(car, straight_dir);

    fprintf('[10/10] Pistas completas\n');
    tracks = {
        'autox',            1, 'trackAutoX';
        'skidpad',          2, 'trackSkidPad';
        'acceleration',     3, 'trackAcceleration';
        'autox2024v1',      4, 'track_AutoX_2024_V1';
        'autox2024v1_rev',  4, 'track_AutoX_2024_V1_reversed';
    };
    for k = 1:size(tracks, 1)
        name = tracks{k, 1};
        fprintf('   -> %s\n', name);
        t = export_track(car, fullfile(out_dir, 'tracks', name), tracks{k, 2}, tracks{k, 3});
        manifest.timings_s.(['sim_' name]) = t;
    end

    fid = fopen(fullfile(out_dir, 'manifest.json'), 'w');
    fprintf(fid, '%s\n', jsonencode(manifest, 'PrettyPrint', true));
    fclose(fid);

    close all force;
    fprintf('Referencia gerada em: %s\n', out_dir);
end

% =========================================================================
%  Seções
% =========================================================================

function export_math(d)
    % linspace: mesmos formatos usados no runSimulation (início em 0, fim decimal)
    ends = [0.37, 5.123, 51.5, 1.4, 0.1, 2.9999, 123.456789, 7.15e-3, 1e5/3];
    ns = [1, 2, 3, 4, 7, 10, 25, 103, 1001];
    rows = zeros(0, 5);
    c = 0;
    for b = ends
        for n = ns
            c = c + 1;
            y = linspace(0, b, n);
            rows = [rows; repmat([c, 0, b, n], n, 1), y(:)]; %#ok<AGROW>
        end
    end
    % alguns casos com início diferente de zero
    for ab = [1.5, 9.75; -3.2, 4.4; 10, 2]'
        for n = [2, 5, 33]
            c = c + 1;
            y = linspace(ab(1), ab(2), n);
            rows = [rows; repmat([c, ab(1), ab(2), n], n, 1), y(:)]; %#ok<AGROW>
        end
    end
    write_csv(fullfile(d, 'linspace.csv'), {'case', 'a', 'b', 'n', 'value'}, rows);

    % interp1 linear com extrapolação (tabela de Cp do calculateBrakeTemp)
    T_ref = [25, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000];
    Cp_ref = [490, 515, 550, 580, 605, 630, 655, 680, 705, 725, 745];
    T = unique([linspace(-50, 1200, 2501)'; T_ref'; 25.000000001; 999.99999]);
    write_csv(fullfile(d, 'interp1_linear_extrap.csv'), {'x', 'y'}, ...
              [T, interp1(T_ref, Cp_ref, T, 'linear', 'extrap')]);

    % griddedInterpolant pchip / linear em conjuntos pequenos (casos de borda)
    sets = {
        [0, 1],             [2, 5];             % 2 pontos
        [0, 1, 3],          [0, 2, 1];          % 3 pontos, muda de sinal
        [0, 1, 2, 4, 5],    [1, 1, 3, 3, 0];    % patamares (derivada zero)
        [-2, 0.5, 1, 7],    [4, -1, -1.5, 10];  % espaçamento irregular
    };
    rows = zeros(0, 4);
    for s = 1:size(sets, 1)
        x = sets{s, 1}; y = sets{s, 2};
        xq = linspace(x(1) - 1.5, x(end) + 1.5, 101)';
        xq = unique([xq; x(:)]);
        gp = griddedInterpolant(x, y, 'pchip');
        gl = griddedInterpolant(x, y, 'linear');
        rows = [rows; repmat(s, numel(xq), 1), xq, gp(xq), gl(xq)]; %#ok<AGROW>
    end
    write_csv(fullfile(d, 'gridded_small.csv'), {'set', 'x', 'pchip', 'linear'}, rows);
end

function export_car(car, d)
    % Todos os parâmetros numéricos do carro (escalares e vetores).
    f = fieldnames(car);
    keys = {}; vals = [];
    for k = 1:numel(f)
        v = car.(f{k});
        if isnumeric(v) || islogical(v)
            if isscalar(v)
                keys{end+1} = f{k}; vals(end+1) = double(v); %#ok<AGROW>
            else
                for j = 1:numel(v)
                    keys{end+1} = sprintf('%s(%d)', f{k}, j); vals(end+1) = double(v(j)); %#ok<AGROW>
                end
            end
        end
    end
    write_kv(fullfile(d, 'car_params.csv'), keys, vals);

    % Dados brutos dos interpoladores
    rpm_t = car.TorqueInterpol.GridVectors{1}(:);
    write_csv(fullfile(d, 'engine_torque_data.csv'), {'rpm', 'torque_nm'}, ...
              [rpm_t, car.TorqueInterpol.Values(:)]);
    rpm_b = car.bsfc_map.GridVectors{1}(:);
    write_csv(fullfile(d, 'engine_bsfc_data.csv'), {'rpm', 'bsfc_g_kwh'}, ...
              [rpm_b, car.bsfc_map.Values(:)]);

    % Avaliação dentro e FORA da faixa (extrapolação pchip / linear)
    mids = (rpm_t(1:end-1) + rpm_t(2:end)) / 2;
    rpm = unique([linspace(3000, 14000, 2201)'; rpm_t; mids; rpm_b; 8600]);
    write_csv(fullfile(d, 'engine_eval.csv'), {'rpm', 'torque_nm', 'bsfc_g_kwh'}, ...
              [rpm, car.TorqueInterpol(rpm), car.bsfc_map(rpm)]);

    write_kv(fullfile(d, 'scalars.csv'), ...
             {'launch_torque_nm', 'brake_force_n'}, ...
             [calculateLaunchTorque(car), calculateBrakeForce(car)]);
end

function export_primitives(car, d)
    vs = 0:0.5:40;
    slopes = [-0.2, -0.05, 0, 0.05, 0.17];
    M = zeros(numel(vs) * numel(slopes), 6); r = 0;
    for v = vs
        for s = slopes
            r = r + 1;
            F_down = calculateDownforce(car, v);
            M(r, :) = [v, s, calculateDrag(car, v), F_down, ...
                       calculateSlopeForce(car, s), calculateTyreResistance(car, v, s, F_down)];
        end
    end
    write_csv(fullfile(d, 'primitives.csv'), ...
              {'v', 'slope', 'drag', 'downforce', 'slope_force', 'rolling_resistance'}, M);
end

function export_load_transfer(car, d)
    vs = 0:2:40;
    slopes = [-0.2, 0, 0.17];
    axs = [-2, -1, 0, 0.5, 1.2];
    ays = [-1.5, 0, 1.5];
    M = zeros(numel(vs) * numel(slopes) * numel(axs) * numel(ays), 20); r = 0;
    for v = vs
        for s = slopes
            for ax = axs
                for ay = ays
                    r = r + 1;
                    S = calculateStaticLoads(car, s);
                    T = calculateDynamicTransfer(car, ax, ay);
                    [N, D] = calculateLoadTransfer(car, ax, ay, s, v);
                    M(r, :) = [v, s, ax, ay, S', T', N', D'];
                end
            end
        end
    end
    write_csv(fullfile(d, 'load_transfer.csv'), ...
              {'v', 'slope', 'ax_g', 'ay_g', ...
               'static1', 'static2', 'static3', 'static4', ...
               'dyn1', 'dyn2', 'dyn3', 'dyn4', ...
               'normal1', 'normal2', 'normal3', 'normal4', ...
               'dist1', 'dist2', 'dist3', 'dist4'}, M);
end

function export_powertrain(car, d)
    vs = 0:0.25:45;
    ng = numel(car.gear_ratios);
    M = zeros(numel(vs) * ng, 6); r = 0;
    for v = vs
        for g = 1:ng
            r = r + 1;
            rpm = calculateEngineRPM(car, v, g);
            wt = calculateEngineTorque(car, rpm, g);
            M(r, :) = [v, g, rpm, wt, calculateEngineForce(car, wt), ...
                       updateEngineState(car, v, g, false, 0, 0)];
        end
    end
    write_csv(fullfile(d, 'powertrain.csv'), ...
              {'v', 'gear', 'rpm', 'wheel_torque', 'engine_force', 'motor_rpm'}, M);

    vs = 0:0.1:45;
    M = zeros(numel(vs), 2);
    for k = 1:numel(vs)
        M(k, :) = [vs(k), findInitialGear(car, vs(k))];
    end
    write_csv(fullfile(d, 'initial_gear.csv'), {'v', 'gear'}, M);

    Fs = 0:250:12000;
    Ns = [0, 500, 1000, 1500, 2000, 3000];
    M = zeros(numel(Fs) * numel(Ns), 3); r = 0;
    for F = Fs
        for N = Ns
            r = r + 1;
            M(r, :) = [F, N, calculateMaxGrip(car, F, N)];
        end
    end
    write_csv(fullfile(d, 'max_grip.csv'), {'engine_force', 'normal_rear', 'max_grip'}, M);
end

function export_brake_temp(car, d)
    vs = [0, 0.05, 5, 15, 30];
    Fb = [0, 3000, 13000];
    Ts = [20, 25, 60, 61, 300, 900, 1100];
    dts = [0.01, 0.05];
    ratios = [0.5, 0.6, 0.7];
    M = zeros(numel(vs) * numel(Fb) * numel(Ts) * numel(dts) * numel(ratios), 11); r = 0;
    for v = vs
        for F = Fb
            for T = Ts
                for dt = dts
                    for q = ratios
                        r = r + 1;
                        [T1, Pin, Pc, Pr, Pk, h] = calculateBrakeTemp(car, v, F, T, dt, q);
                        M(r, :) = [v, F, T, dt, q, T1, Pin, Pc, Pr, Pk, h];
                    end
                end
            end
        end
    end
    write_csv(fullfile(d, 'brake_temp.csv'), ...
              {'v', 'brake_force', 'temp', 'dt', 'front_ratio', ...
               'new_temp', 'p_in', 'p_conv', 'p_rad', 'p_cond', 'h'}, M);
end

function export_gearbox(car, d)
    dt = 0.01;
    shift_states = [0, 0; 1, 0.12; 1, 0.005; 1, 0.01; 1, -0.001];
    ng = numel(car.gear_ratios);

    % manageGear (inclui checkUpshift / checkDownshift)
    vs = 0:1:40;
    M = zeros(numel(vs) * ng * 2 * size(shift_states, 1), 8); r = 0;
    for v = vs
        for g = 1:ng
            for acc = [0, 1]
                for s = 1:size(shift_states, 1)
                    r = r + 1;
                    st = struct('isShifting', logical(shift_states(s, 1)), 'timer', shift_states(s, 2));
                    [ng_out, st_out] = manageGear(car, v, g, logical(acc), st, dt);
                    M(r, :) = [v, g, acc, shift_states(s, :), ng_out, st_out.isShifting, st_out.timer];
                end
            end
        end
    end
    write_csv(fullfile(d, 'manage_gear.csv'), ...
              {'v', 'gear', 'is_accelerating', 'is_shifting', 'timer', ...
               'new_gear', 'new_is_shifting', 'new_timer'}, M);

    % calculateForces
    shift_states = [0, 0; 1, 0.05; 1, 0.01];
    slopes = [0, 0.1];
    vs = 0:1:40;
    M = zeros(numel(vs) * numel(slopes) * ng * 2 * size(shift_states, 1), 14); r = 0;
    for v = vs
        for s = slopes
            for g = 1:ng
                for acc = [0, 1]
                    for k = 1:size(shift_states, 1)
                        r = r + 1;
                        st = struct('isShifting', logical(shift_states(k, 1)), 'timer', shift_states(k, 2));
                        [Fe, Fd, Fr, Fs, g2, st2, Fdown] = calculateForces(car, v, s, g, logical(acc), st, dt, 0);
                        M(r, :) = [v, s, g, acc, shift_states(k, :), ...
                                   Fe, Fd, Fr, Fs, g2, st2.isShifting, st2.timer, Fdown];
                    end
                end
            end
        end
    end
    write_csv(fullfile(d, 'calculate_forces.csv'), ...
              {'v', 'slope', 'gear', 'is_accelerating', 'is_shifting', 'timer', ...
               'engine_force', 'drag', 'rolling_resistance', 'slope_force', ...
               'new_gear', 'new_is_shifting', 'new_timer', 'downforce'}, M);
end

function export_longitudinal(car, d)
    dt = 0.01;
    vs = [0, 0.5, 1, 5, 10, 20, 30, 40];
    slopes = [-0.1, 0, 0.1];

    Fe = [0, 500, 2000, 4000, 6000, 8000, 12000];
    M = zeros(numel(vs) * numel(Fe) * numel(slopes), 6); r = 0;
    for v = vs
        for F = Fe
            for s = slopes
                r = r + 1;
                [drag, roll, sf, down] = resistances(car, v, s);
                [p, v1, a] = carAccelerate(car, F, drag, roll, sf, v, 0, dt, s, down);
                M(r, :) = [v, F, s, p, v1, a];
            end
        end
    end
    write_csv(fullfile(d, 'car_accelerate.csv'), {'v', 'engine_force', 'slope', 'pos', 'vel', 'acc'}, M);

    Fb = [calculateBrakeForce(car), 1000, 5000, 30000];
    M = zeros(numel(vs) * numel(Fb) * numel(slopes), 7); r = 0;
    for v = vs
        for F = Fb
            for s = slopes
                r = r + 1;
                [drag, roll, sf, down] = resistances(car, v, s);
                [p, v1, a, fb] = carBrake(car, F, drag, roll, sf, v, 0, dt, s, down);
                M(r, :) = [v, F, s, p, v1, a, fb];
            end
        end
    end
    write_csv(fullfile(d, 'car_brake.csv'), {'v', 'brake_force', 'slope', 'pos', 'vel', 'acc', 'actual_brake_force'}, M);
end

function export_corner_speed(car, d)
    Rs = [3, 5, 6, 8.2, 10, 15, 20, 30, 50, 92.9899, 130, 136, 136.5, 137, 200, 1000];
    slopes = [0, 0.001, -0.0015, 0.0017, 0.05];
    M = zeros(numel(Rs) * numel(slopes), 3); r = 0;
    for R = Rs
        for s = slopes
            r = r + 1;
            M(r, :) = [R, s, calculateMaxSafeCurveSpeed(car, R, s, 0)];
        end
    end
    write_csv(fullfile(d, 'corner_speed.csv'), {'radius', 'slope', 'v_max'}, M);
end

function elapsed = export_straights(car, d)
    % L, v0, target, slope, trackType, T0
    cases = [
        75,   0,   inf,  0,     3, 25;
        50,   0,   15,   0,     1, 25;
        100,  20,  12,   0.05,  1, 80;
        30,   25,  10,  -0.1,   1, 300;
        200,  10,  25,   0,     1, 25;
        10,   5,   inf,  0.1,   1, 25;
        5,    0,   9.5,  0,     2, 25;
        150,  30,  30,   0,     1, 25;
        60,   12,  inf, -0.05,  1, 150;
        40,   28,  5,    0,     1, 500;
    ];
    write_csv(fullfile(d, 'cases.csv'), {'length', 'v0', 'target', 'slope', 'track_type', 'temp0'}, cases);
    totals = zeros(size(cases, 1), 2);
    tic;
    for k = 1:size(cases, 1)
        c = cases(k, :);
        [t, h] = calculate_Straight(car, c(1), c(2), c(3), c(4), c(5), c(6));
        totals(k, :) = [k, t];
        write_struct_of_vectors(fullfile(d, sprintf('case_%02d.csv', k)), h);
    end
    elapsed = toc;
    write_csv(fullfile(d, 'totals.csv'), {'case', 'total_time'}, totals);
end

function elapsed = export_track(car, d, track_type, fn)
    ensure_dir(d);
    segments = feval(fn);
    write_csv(fullfile(d, 'segments.csv'), {'index', 'length', 'radius', 'slope', 'type'}, segments);

    [X, Y, Z, TV] = buildTrack(segments);
    close all force;
    write_csv(fullfile(d, 'geometry.csv'), {'x', 'y', 'z'}, [X(:), Y(:), Z(:)]);
    write_csv(fullfile(d, 'track_vectors.csv'), {'u', 'v'}, TV);

    [vc, sl, TL] = initializeTrack(car, segments);
    write_csv(fullfile(d, 'corners.csv'), {'corner_velocity', 'slope', 'length'}, [vc, sl, TL]);

    % Simulação completa via runSimulation em modo otimização (sem gráficos).
    % Ele salva Optimization_Result.mat na pasta atual -> roda numa pasta temporária.
    tmp = tempname; mkdir(tmp);
    old = cd(tmp);
    restore = onCleanup(@() cd(old));
    tic;
    runSimulation(true, track_type, fn);
    elapsed = toc;
    S = load(fullfile(tmp, 'Optimization_Result.mat'), 'SimOut');
    clear restore;
    close all force;

    write_struct_of_vectors(fullfile(d, 'history.csv'), S.SimOut.History);
    rf = fieldnames(S.SimOut.Results);
    rv = cellfun(@(f) double(S.SimOut.Results.(f)), rf);
    write_kv(fullfile(d, 'results.csv'), rf, rv);
end

% =========================================================================
%  Utilitários
% =========================================================================

function [drag, roll, sf, down] = resistances(car, v, s)
    down = calculateDownforce(car, v);
    drag = calculateDrag(car, v);
    roll = calculateTyreResistance(car, v, s, down);
    sf = calculateSlopeForce(car, s);
end

function ensure_dir(d)
    if ~isfolder(d), mkdir(d); end
end

function write_csv(path, header, M)
    fid = fopen(path, 'w');
    assert(fid > 0, 'Nao foi possivel abrir %s', path);
    c = onCleanup(@() fclose(fid));
    fprintf(fid, '%s\n', strjoin(header, ','));
    if ~isempty(M)
        fmt = [strjoin(repmat({'%.17g'}, 1, numel(header)), ','), '\n'];
        fprintf(fid, fmt, double(M).');
    end
end

function write_kv(path, keys, vals)
    fid = fopen(path, 'w');
    assert(fid > 0, 'Nao foi possivel abrir %s', path);
    c = onCleanup(@() fclose(fid));
    fprintf(fid, 'key,value\n');
    for k = 1:numel(keys)
        fprintf(fid, '%s,%.17g\n', keys{k}, vals(k));
    end
end

function write_struct_of_vectors(path, S)
    f = fieldnames(S)';
    n = numel(S.(f{1}));
    M = zeros(n, numel(f));
    for k = 1:numel(f)
        v = S.(f{k});
        assert(numel(v) == n, 'Campo %s com tamanho %d (esperado %d)', f{k}, numel(v), n);
        M(:, k) = double(v(:));
    end
    write_csv(path, f, M);
end
