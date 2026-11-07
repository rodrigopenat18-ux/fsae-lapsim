# dados

| Pasta | O que tem |
|---|---|
| `carros/` | carros em JSON. `default.json` é o carro de 2024, o mesmo do `initializeCar.m`. Copie e altere para criar outros, ou use a aba "Carros" da interface. |
| `pistas/` | pistas em CSV (`length_m,radius_m,slope,type`), geradas a partir das funções `track*.m` do MATLAB |
| `privado/` | dados que **não vão para o Git**: logs de aquisição, GPX, exportações do OptimumLap etc. Crie a pasta se quiser usar. |

Os formatos aceitos de pista (CSV do projeto, matriz `segments` do MATLAB e o
`Tipo;Comprimento;Raio` do trackExcel/OptimumLap) e o formato do JSON do carro estão
descritos no [README principal](../README.md).
