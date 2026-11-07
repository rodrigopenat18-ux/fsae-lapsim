# Dados de referência (gabarito do MATLAB)

Gerados por [`tools/matlab/export_reference.m`](../../tools/matlab/export_reference.m) a partir do
simulador MATLAB original em [`matlab/`](../../matlab), **sem modificá-lo**. O port C++ é validado
comparando suas saídas com estes arquivos.

Todos os CSV têm cabeçalho na primeira linha, separador `,` e números com 17 algarismos
significativos (`%.17g`). Valores especiais aparecem como `Inf`, `-Inf` e `NaN`.

| Arquivo | Função MATLAB | Conteúdo |
|---|---|---|
| `manifest.json` | — | versão do MATLAB, data de geração, tempos de execução |
| `unit/car_params.csv` | `initializeCar` | todos os parâmetros numéricos do carro (`key,value`) |
| `unit/engine_torque_data.csv`, `unit/engine_bsfc_data.csv` | `dataEngine` | pontos brutos das curvas |
| `unit/engine_eval.csv` | `griddedInterpolant` | torque (pchip) e BSFC (linear) de 3000 a 14000 rpm, incluindo extrapolação |
| `unit/scalars.csv` | `calculateLaunchTorque`, `calculateBrakeForce` | constantes derivadas |
| `unit/primitives.csv` | `calculateDrag`, `calculateDownforce`, `calculateSlopeForce`, `calculateTyreResistance` | grade de velocidade × slope |
| `unit/load_transfer.csv` | `calculateStaticLoads`, `calculateDynamicTransfer`, `calculateLoadTransfer` | grade v × slope × ax × ay |
| `unit/powertrain.csv` | `calculateEngineRPM`, `calculateEngineTorque`, `calculateEngineForce`, `updateEngineState` | grade v × marcha |
| `unit/initial_gear.csv` | `findInitialGear` | marcha inicial por velocidade |
| `unit/max_grip.csv` | `calculateMaxGrip` | grade força × normal |
| `unit/brake_temp.csv` | `calculateBrakeTemp` | grade v × força × temperatura × dt × distribuição |
| `unit/manage_gear.csv` | `manageGear`, `checkUpshift`, `checkDownshift` | um passo do câmbio |
| `unit/calculate_forces.csv` | `calculateForces` | um passo de forças + câmbio |
| `unit/car_accelerate.csv`, `unit/car_brake.csv` | `carAccelerate`, `carBrake` | um passo de integração |
| `unit/corner_speed.csv` | `calculateMaxSafeCurveSpeed` | grade raio × slope |
| `straight/cases.csv`, `straight/case_NN.csv`, `straight/totals.csv` | `calculate_Straight` | casos de reta com histórico completo |
| `tracks/<pista>/segments.csv` | funções `track*` | matriz de segmentos |
| `tracks/<pista>/geometry.csv`, `track_vectors.csv` | `buildTrack` | X, Y, Z e vetores de direção |
| `tracks/<pista>/corners.csv` | `initializeTrack` | velocidade máxima por segmento |
| `tracks/<pista>/history.csv` | `runSimulation` | histórico completo da simulação |
| `tracks/<pista>/results.csv` | `runSimulation` | `SimOut.Results` |

Pistas: `autox`, `skidpad`, `acceleration`, `autox2024v1`, `autox2024v1_rev`.

## Regenerar

```bash
matlab -batch "addpath('tools/matlab'); export_reference"
```
