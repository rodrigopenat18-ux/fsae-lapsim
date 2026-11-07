# Problemas conhecidos (herdados do MATLAB)

O port C++ reproduz **fielmente** o simulador MATLAB, inclusive os comportamentos abaixo,
para que os resultados possam ser validados contra a referência. Cada item será corrigido
depois, um de cada vez, como opção (desligada por padrão) e com o impacto medido no tempo de volta.

| # | Onde (MATLAB) | Problema | Efeito |
|---|---|---|---|
| 1 | `initializeTrack.m` | `theta = atan(slope/100)` trata o slope como %, mas o resto do código usa radianos | Velocidade de curva calculada como se a pista fosse plana |
| 2 | `calculateMaxSafeCurveSpeed.m` | Denominador `m − μ·K_aero·R` vai a zero perto de R ≈ 136,5 m e fica negativo depois; `max(0, …)` zera `v_grip` | Perto do limite a velocidade explode (R = 136,5 m → 2.176 m/s); acima dele a curva fica com velocidade 0. Afeta pistas em CSV com raios grandes: em `Pista_AutoX.csv`, 70 dos 544 segmentos têm R > 136,5 m — 69 ficam com velocidade 0 (o carro passa a 0,1 m/s e a volta leva ~8.400 s) e um (R = 136,527 m) fica com 3.367 m/s. O `fsae_sim run` avisa quando isso acontece |
| 3 | `calculate_Straight.m` + `runSimulation.m` | Cada reta termina em `pos ≥ L` (ultrapassa até v·dt); o corte da volta 1 usa `distance >= comprimento_pista` | Distância acumulada adiantada; volta 1 cortada cedo e `Lap2_Time` inflado. Como a reta final se funde com a reta de largada da volta seguinte, a divisão cai no meio de uma reta: com 4 voltas no autocross, as voltas lançadas saem 45,43 / 45,42 / 45,57 s em vez de iguais. A última volta não tem reta seguinte para fundir e sai ~1 s mais lenta (25 voltas: 45,41 s nas intermediárias, 46,44 s na última) — precisa ser resolvido antes do endurance |
| 4 | `checkDownshift.m` | Reduz marcha sempre que rpm < `downshiftRPM` (11500) sem checar o rpm resultante | RPM gravado passa do limite (até ~14.000 rpm) na frenagem |
| 5 | `calculateDynamicTransfer.m` / `runSimulation.m` | Convenção de sinal da aceleração lateral contradiz os comentários | Sem efeito numérico (checagem simétrica); só o sinal da telemetria |
| 6 | `dataEngine.m` | `griddedInterpolant` pchip extrapola a curva de torque fora de 4.499–12.454 rpm sem limite | Valores absurdos (689 Nm a 3.000 rpm; 8.561 Nm a 14.000 rpm). Hoje só ocorre nas reduções de marcha da frenagem (item 4), cujo torque é descartado; vira problema se a física mudar |

## Simplificações de modelo (melhorias futuras, não são bugs)

- O carro nunca acelera dentro de curva; em sequência de curvas a velocidade só diminui.
- Redução de velocidade entre curvas consecutivas é instantânea (sem frenagem).
- Tração cai de μ estático para μ cinético quando a força do motor passa da aderência estática.
- Não há modo endurance (planejado: autocross + 25 voltas). O C++ já aceita qualquer número de voltas (`--laps`).

## Observações sobre os dados das pistas

- `track_AutoX_2024_V1.m`: aplicando o GPX da equipe (`fsae_sim gpx-slope`) à geometria
  original, os slopes dos segmentos 1–31 batem com a matriz até a 6ª casa decimal.
  Os segmentos 32–35 (o antigo segmento 32 dividido em quatro) e o 52 foram editados à
  mão e não batem: o segmento 32 tem `+0.089364`, e o GPX dá exatamente `−0.089364`
  (provável sinal trocado). Os dados foram mantidos como estão no MATLAB.

## Corrigido no port (não afeta a física)

- `loadRealLog.m`: o tipo 1 procurava `log_autocross.csv`, que não existe (arquivo real:
  `log_autox.csv`). Agora o arquivo é passado explicitamente (`tools/matlab/loadRealLogFile.m`,
  `plot_results.py --log`).
- `loadRealLog.m`: o apelido `DISTANCIA` não casava com a coluna `Distância` por causa do
  acento, e a distância era sempre recalculada; os nomes agora são comparados sem acentos.
- Comparação com log no Python: o log também é sincronizado no primeiro instante com
  v > 0,5 m/s (o `plotComparison.m` só sincronizava a simulação, e o log aparecia
  deslocado pelo tempo do datalogger, ex.: começando em 152 s).
