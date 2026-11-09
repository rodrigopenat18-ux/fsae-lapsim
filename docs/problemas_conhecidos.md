# Problemas conhecidos do modelo

O modelo **original** (o padrão, idêntico à versão em MATLAB) tem os comportamentos abaixo.
As correções existem como opções (`fsae::ModelOptions`, em
[`Modules/Vehicle/model_options.hpp`](../Modules/Vehicle/model_options.hpp)), que podem ser
ligadas uma a uma ou todas juntas (`ModelOptions::corrected()`), com testes em
[`Tests/test_model_fixes.cpp`](../Tests/test_model_fixes.cpp).

| # | Onde | Problema | Efeito no original | Correção (opção) |
|---|---|---|---|---|
| 1 | velocidade de curva (`initializeTrack`) | `theta = atan(slope/100)` trata a inclinação como %, mas o resto do código usa radianos | Velocidade de curva calculada como se a pista fosse plana | `slope_in_radians`: usa radianos. Efeito: +0,005 s (autox 2024 V1) e +0,036 s (V1 inversa) na volta 1, já que a componente do peso que aperta o pneu cai com cos θ |
| 2 | `calculateMaxSafeCurveSpeed` | Quando `m − μ·K_aero·R ≤ 0` (R ≥ 136,5 m no carro de 2024), o `max(0, …)` zera `v_grip` | Curvas de raio grande ficam com velocidade 0. Em `Pista_AutoX.csv`, 69 curvas; o carro passa a 0,1 m/s e a volta leva ~8.400 s | `unbounded_corner_grip`: o downforce cresce mais rápido que a força centrípeta necessária, então a curva **não tem limite de aderência**. Na `Pista_AutoX.csv`, a volta lançada cai de 8.392 s para 90 s; o que ainda a deixa lenta é o item "não acelera em curva", abaixo |
| 3 | reta (`calculate_Straight`) e divisão das voltas | Cada reta termina em `pos ≥ L` (passa do fim em até v·dt), e o corte da volta usa a primeira amostra com `distance ≥ k·L` | A distância acumula ~1 m a mais por volta (+27 m em 25 voltas), e as voltas são cortadas cedo. A última de um endurance sai ~1 s mais lenta. No skidpad, o carro "freava" um passo inteiro para cobrir os últimos 0,8 mm e entrava na curva a 9,83 m/s em vez de 10,05 m/s | `exact_segment_end`: o último passo é encurtado até o fim exato do segmento e o tempo da volta é interpolado no cruzamento da linha. A distância passa a ser exata; skidpad −0,125 s; diferença entre as voltas lançadas em 25 voltas cai de 1,03 s para 0,29 s. O que sobra vem da busca do ponto de frenagem, que só começa a frear no início de um passo de 0,01 s e aceita terminar até 0,1 m/s abaixo do alvo |
| 4 | `checkDownshift` | Reduz marcha sempre que rpm < `downshiftRPM` (11500), sem checar o rpm resultante | Rotação chega a ~14.000 rpm na frenagem, acima do limite do motor (12.454 rpm) | `downshift_rev_check`: só reduz se a marcha de baixo não passar do rpm de troca dela. A rotação máxima fica em ~12.030 rpm; o tempo de volta não muda, porque na frenagem a força do motor não é usada |
| 5 | `calculateDynamicTransfer` / `runSimulation` | Convenção de sinal da aceleração lateral contradiz os comentários | Sem efeito numérico (checagem simétrica); só o sinal da telemetria | — |
| 6 | `dataEngine` | A interpolação pchip extrapola a curva de torque fora de 4.499–12.454 rpm sem limite | Valores absurdos (689 N·m a 3.000 rpm; 8.561 N·m a 14.000 rpm). Nas pistas, isso só aparece nas reduções da frenagem, cujo torque é descartado | `limit_engine_map`: torque 0 acima da rotação máxima (corte do limitador), torque do primeiro ponto abaixo da mínima, BSFC constante fora da faixa. Sem efeito nas pistas atuais; protege o modelo quando a física mudar |

## Simplificações de modelo (melhorias planejadas)

- O carro nunca acelera dentro de curva; em sequência de curvas a velocidade só diminui.
- Redução de velocidade entre curvas consecutivas é instantânea (sem frenagem).
- A frenagem e a aceleração não dividem a aderência com a curva (sem elipse de atrito).
- Tração cai de μ estático para μ cinético quando a força do motor passa da aderência estática.
- O pneu tem atrito constante (sem sensibilidade à carga).
- Não há modo endurance completo (planejado: autocross + 25 voltas, com a massa caindo pelo
  combustível consumido). O número de voltas já é parâmetro (`--laps`).

## Observações sobre os dados das pistas

- `track_AutoX_2024_V1.m`: aplicando o GPX da equipe (`fsae_sim gpx-slope`) à geometria
  original, os slopes dos segmentos 1–31 batem com a matriz até a 6ª casa decimal.
  Os segmentos 32–35 (o antigo segmento 32 dividido em quatro) e o 52 foram editados à
  mão e não batem: o segmento 32 tem `+0.089364`, e o GPX dá exatamente `−0.089364`
  (provável sinal trocado). Os dados foram mantidos como estão.

## Corrigido na conversão (não afeta a física)

- `loadRealLog.m`: o tipo 1 procurava `log_autocross.csv`, que não existe (arquivo real:
  `log_autox.csv`). Agora o arquivo é passado explicitamente (`MATLAB/Tools/loadRealLogFile.m`,
  `plot_results.py --log`).
- `loadRealLog.m`: o apelido `DISTANCIA` não casava com a coluna `Distância` por causa do
  acento, e a distância era sempre recalculada; os nomes agora são comparados sem acentos.
- Comparação com log no Python: o log também é sincronizado no primeiro instante com
  v > 0,5 m/s (o `plotComparison.m` só sincronizava a simulação, e o log aparecia
  deslocado pelo tempo do datalogger, ex.: começando em 152 s).
