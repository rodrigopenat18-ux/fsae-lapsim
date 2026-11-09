# Modelo melhorado (quase-estático em distância)

Código: [`Modules/Simulation/quasi_static.cpp`](../Modules/Simulation/quasi_static.cpp).
Testes: [`Tests/test_quasi_static.cpp`](../Tests/test_quasi_static.cpp).
Escolha do modelo: `SimulationOptions::solver` (`Solver::Original` ou `Solver::QuasiStatic`).

O modelo original (idêntico ao MATLAB) continua disponível e é o padrão da biblioteca. O
melhorado trata as simplificações listadas em [problemas_conhecidos.md](problemas_conhecidos.md)
e já inclui todas as correções pontuais (`ModelOptions::corrected()`).

## Como funciona

1. **Malha em distância.** Cada segmento é dividido em passos de no máximo
   `step_m` (padrão 0,25 m), com um ponto exatamente em cada fronteira de segmento e de volta.
2. **Limite de cada curva.** A maior velocidade, subindo a partir de zero, em que
   - cada eixo segura a sua parte da força lateral, `Fy_d = m·ay·(1 − w)` e
     `Fy_t = m·ay·w` (equilíbrio de momentos em regime; `w` = fração do peso atrás);
   - a traseira ainda consegue vencer arrasto, rolamento e rampa dentro da elipse de atrito;
   - nenhuma roda fica descarregada (tombamento).

   Se nada limita até a velocidade máxima do carro (última marcha no limitador), a curva
   não tem limite.
3. **Passada para trás (envelope de frenagem).** Do fim para o começo, a maior velocidade
   que ainda permite frear até o ponto seguinte, com a desaceleração máxima do estado
   (preditor-corretor: média da desaceleração nas duas pontas do passo).
4. **Passada para frente.** Do começo para o fim, plena carga com o motor, o câmbio e o
   tempo de troca, sem passar do envelope. Onde o envelope limita, o carro freia ou alivia
   o acelerador. O tempo de cada passo é `2·ds/(v₀ + v₁)` (aceleração constante no passo).

## Física

| Item | Modelo |
|---|---|
| Atrito do pneu | `μ = μ₀·(1 − k·(Fz/Fz_estática − 1))` em cada pneu; `k = tyre.load_sensitivity` (0 = atrito constante) |
| Elipse de atrito | força longitudinal disponível num eixo: `Fx_max·sqrt(1 − (Fy/Fy_max)²)` |
| Tração | só no eixo traseiro, com controle de tração ideal (sem queda para μ cinético) |
| Aceleração em curva | limitada pela elipse traseira e pela dianteira, que perde carga ao acelerar (sai de frente) |
| Frenagem | dois eixos. Distribuição **ideal** por padrão: cada eixo no seu limite, seguindo a transferência de carga longitudinal. Opcional: balanço fixo `brakes.bias_front_percent`, em que o eixo que chega antes ao limite define a força |
| Frenagem em curva | limitada pela elipse dos dois eixos e pela traseira, que perde carga ao frear (sai de traseira) |
| Torque de freio | `brakes.max_torque_nm` (padrão 3000 N·m, como no original) |
| Motor | mapa limitado à faixa medida; abaixo do rpm de largada a embreagem patina nele |
| Câmbio | sobe no rpm de troca, com `shift_time` sem força do motor; reduz ao frear para a marcha de maior força que não passa do rpm de troca, sem custo de tempo |
| Combustível | potência realmente usada: `F_motor·v/η`, com o BSFC na rotação do motor |
| Freio (temperatura) | potência de frenagem dividida entre os eixos como no modelo de freio acima |

Com os dados de 2024 (μx = 2,1; h/L = 0,20), a distribuição ideal fica perto de 90% na
dianteira em frenagem forte. Um balanço fixo nunca supera a distribuição ideal.

## Efeito (carro de 2024)

| Pista | Original (volta 1 / 2) | Melhorado (volta 1 / 2) |
|---|---|---|
| autox | 47,492 / 45,501 s | 47,087 / 45,019 s |
| autox 2024 V1 | 71,661 / 70,113 s | 69,399 / 67,961 s |
| autox 2024 V1 inversa | 73,069 / 71,668 s | 69,522 / 67,838 s |
| skidpad | 6,240 s | 6,075 s |
| aceleração | 4,250 s | 4,247 s |

- O ganho vem principalmente de acelerar dentro das curvas e de não perder velocidade
  instantaneamente entre curvas seguidas. Na aceleração, que é limitada pelo motor, quase
  nada muda.
- Rotação máxima: ~14.000 → 12.000 rpm (sem sobregiro nas reduções).
- Consumo menor (−7% a −13%): o original contava plena carga o tempo todo nas retas e a
  potência para manter a velocidade nas curvas; o melhorado conta só a potência usada.
- 25 voltas no autox: as voltas lançadas ficam idênticas (diferença de 0,29 s no original
  com as correções) e a distância é exata.
- O uso da elipse de atrito fica em no máximo 1,01 nas pistas planas do projeto com passo de
  0,25 m (1,05 numa pista de GPS com segmentos de 1 a 3 m e curvatura mudando a cada um).
  Esse pequeno excesso é a força média do passo amostrada no fim dele, e cai para 1,001
  com passo de 0,05 m. O tempo de volta muda menos de 0,01% entre os dois passos.

## Limitações

- Quase-estático: não há dinâmica de guinada, de rolagem nem transitórios de pneu.
- A transferência lateral se divide igualmente entre os eixos (como no original).
- O BSFC é o de plena carga também em carga parcial.

## Endurance

Prova `endurance` (`--event endurance` ou "Endurance" na interface): 25 voltas seguidas,
largada parada, temperatura do freio contínua entre as voltas. Com `--fuel <kg>`
(combustível na largada), a massa de cada volta é a do carro mais o combustível que resta
no meio dela, supondo o tanque no CG (a distribuição de peso não muda):

1. uma primeira passada com o tanque cheio em todas as voltas dá o consumo de cada volta;
2. a segunda passada usa a massa de cada volta calculada com esse consumo.

Uma terceira passada muda o tempo total em menos de 1 ms (teste em
`Tests/test_endurance.cpp`), então uma correção basta. No autox, com 4 kg de combustível,
a massa cai de 298,9 para 296,0 kg, as voltas lançadas ficam ~2 ms mais rápidas a cada
volta (45,091 → 45,037 s) e o consumo total é de 3,1 kg em 26 km. A troca de piloto não é
simulada. No modelo original, o combustível entra como massa constante.
