# simulador

A física do simulador em C++17, compilada como a biblioteca `fsae::core`. É usada pela
interface gráfica, pela linha de comando e pelos testes.

| Pasta | O que tem | Equivalente no MATLAB |
|---|---|---|
| `carro/` | parâmetros do carro (`Car`, `CarSpec`) e mapas do motor | `initializeCar.m`, `dataEngine.m` |
| `fisica/` | arrasto, downforce, rampa, rolamento, cargas nos pneus, motor, freio, aderência, temperatura do disco, passo acelerando/freando | `Modules/Physics` |
| `cambio/` | marcha inicial, subida, redução, tempo de troca, two-step | `Modules/Control`, `updateEngineState.m` |
| `pista/` | segmentos, leitura de pistas, velocidade máxima de curva, traçado X/Y/Z, inclinação por GPX | `Modules/Tracks`, `calculateMaxSafeCurveSpeed.m`, `Plot_GPX.m` |
| `simulacao/` | reta completa e volta completa com N voltas | `calculate_Straight.m`, `runSimulation.m` |
| `arquivos/` | JSON do carro, CSV, planilhas Excel, gravação dos resultados | — |
| `matematica/` | interpolação pchip e linear, `linspace` (bit a bit iguais ao MATLAB) | `griddedInterpolant`, `interp1` |

Cada `.hpp` fica ao lado do seu `.cpp`. Os includes partem desta pasta, por exemplo
`#include "fisica/longitudinal.hpp"`. O código usa o namespace `fsae`.

A correspondência função a função está em [`docs/mapa_matlab_cpp.md`](../docs/mapa_matlab_cpp.md).
Ao mexer aqui, mantenha a ordem das operações do MATLAB (ver [`CONTRIBUTING.md`](../CONTRIBUTING.md)).
