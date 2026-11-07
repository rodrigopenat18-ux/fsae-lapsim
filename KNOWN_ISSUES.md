# Problemas conhecidos (herdados do MATLAB)

O port C++ reproduz **fielmente** o simulador MATLAB, inclusive os comportamentos abaixo,
para que os resultados possam ser validados contra a referência. Cada item será corrigido
depois, um de cada vez, como opção (desligada por padrão) e com o impacto medido no tempo de volta.

| # | Onde (MATLAB) | Problema | Efeito |
|---|---|---|---|
| 1 | `initializeTrack.m` | `theta = atan(slope/100)` trata o slope como %, mas o resto do código usa radianos | Velocidade de curva calculada como se a pista fosse plana |
| 2 | `calculateMaxSafeCurveSpeed.m` | Denominador `m − μ·K_aero·R` vai a zero perto de R ≈ 136,5 m e fica negativo depois; `max(0, …)` zera `v_grip` | Perto do limite a velocidade explode (R = 136,5 m → 2.176 m/s); acima dele a curva fica com velocidade 0. Afeta pistas em CSV com raios grandes (70 dos 544 segmentos de `Pista_AutoX.csv`) |
| 3 | `calculate_Straight.m` + `runSimulation.m` | Cada reta termina em `pos ≥ L` (ultrapassa até v·dt); o corte da volta 1 usa `distance >= comprimento_pista` | Distância acumulada adiantada; volta 1 cortada cedo e `Lap2_Time` inflado. Como a reta final se funde com a reta de largada da volta seguinte, a divisão cai no meio de uma reta: com 4 voltas no autocross, as voltas lançadas saem 45,43 / 45,42 / 45,57 s em vez de iguais |
| 4 | `checkDownshift.m` | Reduz marcha sempre que rpm < `downshiftRPM` (11500) sem checar o rpm resultante | RPM gravado passa do limite (~13.300 rpm) na frenagem |
| 5 | `calculateDynamicTransfer.m` / `runSimulation.m` | Convenção de sinal da aceleração lateral contradiz os comentários | Sem efeito numérico (checagem simétrica); só o sinal da telemetria |
| 6 | `dataEngine.m` | `griddedInterpolant` pchip extrapola a curva de torque fora de 4.499–12.454 rpm sem limite | Valores absurdos (689 Nm a 3.000 rpm; 8.561 Nm a 14.000 rpm). Hoje só ocorre nas reduções de marcha da frenagem (item 4), cujo torque é descartado; vira problema se a física mudar |

## Simplificações de modelo (melhorias futuras, não são bugs)

- O carro nunca acelera dentro de curva; em sequência de curvas a velocidade só diminui.
- Redução de velocidade entre curvas consecutivas é instantânea (sem frenagem).
- Tração cai de μ estático para μ cinético quando a força do motor passa da aderência estática.
- Não há modo endurance (planejado: autocross + 25 voltas). O C++ já aceita qualquer número de voltas (`--laps`).

## Corrigido no port (não afeta a física)

- `loadRealLog.m`: o tipo 1 procurava `log_autocross.csv`, que não existe (arquivo real: `log_autox.csv`).
