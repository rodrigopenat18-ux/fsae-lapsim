# Modules

O simulador em C++17, compilado como a biblioteca `fsae::core`. É usado pela interface
gráfica, pela linha de comando e pelos testes.

| Pasta | O que tem |
|---|---|
| `Vehicle/` | parâmetros do carro (`Car`, `CarSpec`) e mapas do motor (torque e BSFC) |
| `Physics/` | arrasto, downforce, rampa, rolamento, cargas nos pneus, motor, freio, aderência, temperatura do disco, passo acelerando/freando |
| `Control/` | câmbio: marcha inicial, subida, redução, tempo de troca, two-step |
| `Tracks/` | segmentos, leitura de pistas, velocidade máxima de curva, traçado X/Y/Z, inclinação por GPX |
| `Simulation/` | reta completa e volta completa com N voltas |
| `IO_Tools/` | JSON do carro, CSV, planilhas Excel, gravação dos resultados |
| `Math/` | interpolação pchip e linear, `linspace` |

Cada `.hpp` fica ao lado do seu `.cpp`. Os includes partem desta pasta, por exemplo
`#include "Physics/longitudinal.hpp"`. O código usa o namespace `fsae`.

Ao mexer aqui, rode os testes (`ctest`): eles mostram na hora se algum resultado mudou.
Veja também o [CONTRIBUTING.md](../CONTRIBUTING.md).
