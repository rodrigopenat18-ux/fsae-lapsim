# Dados de referência (gabarito do MATLAB)

Gerados por [`MATLAB/Scripts/export_reference.m`](../Scripts/export_reference.m) a partir do
simulador MATLAB original em [`MATLAB/Original/`](../Original), **sem modificá-lo**. O port C++ é validado
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
| `import/engine.xlsx` | `initializeCar`/`dataEngine` via [`export_import_fixtures.m`](../Scripts/export_import_fixtures.m) | planilha Excel real (gravada pelo MATLAB) com a curva de torque e as marchas, para testar a importação |
| `gpx/synthetic.gpx`, `gpx/segments_in.csv`, `gpx/slopes.csv`, `gpx/segments_out.csv`, `gpx/z_error.csv` | `applyAltitudeFromGPX` (via [`export_gpx_reference.m`](../Scripts/export_gpx_reference.m)) | GPX sintético sobre a autox 2024 V1 e as inclinações calculadas |

Pistas: `autox`, `skidpad`, `acceleration`, `autox2024v1`, `autox2024v1_rev`.

## Regenerar

```bash
matlab -batch "addpath('MATLAB/Scripts'); export_reference"
```

## Precisão da comparação

O C++ faz as contas na mesma ordem do MATLAB, então quase tudo bate **bit a bit**
(tolerância zero). A exceção conhecida é `pow` com expoente fracionário
(`Re^0.55`, `Re^0.8`, `Re^0.5` no `calculateBrakeTemp`):

- o `pow` escalar do MATLAB R2026a não é o arredondamento correto. Em cerca de 9%
  dos valores ele difere em 1 bit (~2e-16 relativo) do `pow` da biblioteca C, do
  `pow` em precisão estendida e até do `libmmd.dll` (Intel) que acompanha o MATLAB;
- isso aparece em `h_coef`, `P_conv` e `P_rad` (erro relativo ≤ 8e-16) e, por
  acúmulo, em algumas amostras de `brake_temp`/`P_cond` das pistas (≤ 6e-16).
  Essas grandezas alimentam apenas a temperatura do disco, que não afeta a dinâmica:
  tempos, posições, velocidades e marchas continuam idênticos.

Por isso os testes que envolvem `sin`, `cos` ou `pow` usam uma tolerância
relativa pequena (1e-15 a 1e-12), que também cobre as bibliotecas matemáticas de
Linux, macOS e MSVC no CI. Os tempos totais de cada reta são comparados com `==`.

A geometria do traçado (`geometry.csv`, `track_vectors.csv`) acumula `sin`/`cos`
do ângulo ponto a ponto e difere do MATLAB em até ~1e-13 m. Ela serve só para
gráficos e para escolher o ponto de largada, não entra na física. Segmentos e
velocidades máximas de curva (`segments.csv`, `corners.csv`, `corner_speed.csv`)
são idênticos bit a bit.
