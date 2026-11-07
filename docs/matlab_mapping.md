# MATLAB → C++

Correspondência entre o simulador MATLAB ([`matlab/`](../matlab)) e o port C++. Os nomes dos
campos do carro seguem o `initializeCar.m` (ver `include/fsae/vehicle/car.hpp`).

## Carro

| MATLAB | C++ |
|---|---|
| `initializeCar.m` | `fsae::CarSpec::matlab_default()` → `fsae::Car::from_spec()` ([car.hpp](../include/fsae/vehicle/car.hpp)); arquivo [`data/cars/default.json`](../data/cars/default.json) |
| `dataEngine.m` (`TorqueInterpol`, `bsfc_map`) | `fsae::EngineMap::torque()` (pchip) e `bsfc()` (linear) ([engine.hpp](../include/fsae/vehicle/engine.hpp)) |
| `griddedInterpolant(..., 'pchip')` | `fsae::math::PchipInterpolant` ([interp.hpp](../include/fsae/math/interp.hpp)) |
| `griddedInterpolant(..., 'linear')`, `interp1(..., 'linear', 'extrap')` | `fsae::math::LinearInterpolant` |
| `linspace` | `fsae::math::linspace` |
| `updateEngineState.m` | `fsae::control::motor_rpm` ([gearbox.hpp](../include/fsae/control/gearbox.hpp)) |

## Física (`Modules/Physics`)

| MATLAB | C++ |
|---|---|
| `calculateDrag` | `physics::drag` ([resistances.hpp](../include/fsae/physics/resistances.hpp)) |
| `calculateDownforce` | `physics::downforce` |
| `calculateSlopeForce` | `physics::slope_force` |
| `calculateTyreResistance` | `physics::tyre_resistance` |
| `calculateStaticLoads` | `physics::static_loads` ([load_transfer.hpp](../include/fsae/physics/load_transfer.hpp)) |
| `calculateDynamicTransfer` | `physics::dynamic_transfer` |
| `calculateLoadTransfer` | `physics::load_transfer` |
| `calculateEngineRPM` | `physics::engine_rpm` ([powertrain.hpp](../include/fsae/physics/powertrain.hpp)) |
| `calculateEngineTorque` | `physics::wheel_torque` |
| `calculateEngineForce` | `physics::engine_force` |
| `calculateLaunchTorque` | `physics::launch_torque` |
| `calculateBrakeForce` | `physics::brake_force` |
| `calculateMaxGrip` | `physics::max_grip` |
| `calculateBrakeTemp` | `physics::brake_temperature` ([brake_thermal.hpp](../include/fsae/physics/brake_thermal.hpp)) |
| `calculateForces` | `physics::calculate_forces` ([longitudinal.hpp](../include/fsae/physics/longitudinal.hpp)) |
| `carAccelerate` | `physics::car_accelerate` |
| `carBrake` | `physics::car_brake` |
| `calculate_Straight` | `sim::simulate_straight` ([straight.hpp](../include/fsae/sim/straight.hpp)) |
| `calculateMaxSafeCurveSpeed` | `track::max_safe_corner_speed` ([corner_speed.hpp](../include/fsae/track/corner_speed.hpp)) |
| `carTurn` | não portado (não é chamado em nenhum lugar) |

## Câmbio (`Modules/Control`)

| MATLAB | C++ |
|---|---|
| `findInitialGear` | `control::find_initial_gear` |
| `checkUpshift` | `control::check_upshift` |
| `checkDownshift` | `control::check_downshift` |
| `manageGear` (+ struct `shift_status`) | `control::manage_gear` (+ `control::ShiftState`) |

## Pistas (`Modules/Tracks`)

| MATLAB | C++ |
|---|---|
| `trackAutoX`, `trackSkidPad`, `trackAcceleration`, `track_AutoX_2024_V1`, `track_AutoX_2024_V1_reversed` | arquivos em [`data/tracks/`](../data/tracks) + `track::load_track` ([track.hpp](../include/fsae/track/track.hpp)) |
| `trackExcel` | `track::load_track` (formato `Tipo;Comprimento;Raio`) e `fsae_sim convert-track` |
| `initializeTrack` | `track::corner_speeds` |
| `find_next_corner_speed` | `track::next_corner_speed` |
| `buildTrack` (sem os gráficos) | `track::build_geometry` ([geometry.hpp](../include/fsae/track/geometry.hpp)) |
| escolha da largada por clique (`ginput` + `circshift`) | `track::nearest_segment` + `track::rotate_to_start`; na CLI, `--start-segment` |
| `applyAltitudeFromGPX` (em `Plot_GPX.m`) | `track::apply_gpx_altitude` ([gpx.hpp](../include/fsae/track/gpx.hpp)) e `fsae_sim gpx-slope` |

## Simulação (`runSimulation.m`)

| MATLAB | C++ |
|---|---|
| `runSimulationLoop` | `sim::simulate` ([lap.hpp](../include/fsae/sim/lap.hpp)) |
| `is_multi_lap` (2 voltas fixas) | `SimulationOptions::laps` (`sim::default_laps(Event)` dá 2 em autocross/custom, 1 nas outras) |
| `SimOut.History` | `SimulationResult::history` (`sim::Telemetry`) → `telemetry.csv` |
| `SimOut.Results` | campos de `SimulationResult` → `summary.json` (`matlab_results`) |
| `trackType` 1/2/3/4 | `sim::Event` (`--event autocross\|skidpad\|acceleration\|custom`) |
| `modo_otimizacao` + `Optimization_Result.mat` | toda execução já é sem gráficos e grava arquivos; `--json` imprime o resumo |
| prints de `verbose` | `SimulationOptions::log` / `--verbose` |

## Entrada/saída e gráficos

| MATLAB | C++ / ferramentas |
|---|---|
| `loadRealLog.m` | [`tools/matlab/loadRealLogFile.m`](../tools/matlab/loadRealLogFile.m) (recebe o arquivo; acentos corrigidos) e `plot_results.py --log` |
| `loadOPLapLog.m` | usado como está por [`compareCppResult.m`](../tools/matlab/compareCppResult.m); `plot_results.py --oplap` |
| `plotResults.m` | [`plotCppResult.m`](../tools/matlab/plotCppResult.m) (usa o original) e `plot_results.py` |
| `plotComparison.m` | [`compareCppResult.m`](../tools/matlab/compareCppResult.m) (usa o original) e `plot_results.py --log/--oplap` |
| `plotDashboard.m`, `plotInteractiveTrack.m` | funcionam como estão: `S = loadCppResult(...)`; `plotDashboard(S.History, S.Car)`; `plotInteractiveTrack(S.Track, S.History, S.Car)` |
| `view_optimization.m` | `loadCppResult` devolve um `SimOut` com os mesmos campos que o `.mat` (`History`, `Track`, `Results`, `Car` com o que os gráficos usam) |

## Código do MATLAB que não foi portado

Não é usado em nenhum cálculo: `carTurn`, `PowerInterpol`, os campos `brake_cp`,
`brake_density`, `brake_thickness` e `brake_efficiency`, argumentos não usados
(`current_time`, `downforce` em `carAccelerate`/`carBrake`, `launch_mode`/`launch_timer`, `trackType`
no `calculate_Straight`) e as verificações `time_seg == -1`.
