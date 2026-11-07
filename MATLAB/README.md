# MATLAB (histórico)

O simulador começou em MATLAB. Esta pasta guarda essa versão e o material usado para
convertê-la em C++, **só para consulta**: nada daqui é necessário para usar o simulador.

| Pasta | O que tem |
|---|---|
| `Original/` | o simulador MATLAB original, sem modificações (`root/runSimulation.m`, `Modules/`) |
| `Reference/` | resultados gravados pelo MATLAB. Os testes do C++ ([`Tests/`](../Tests)) comparam o simulador com eles. [Detalhes da comparação](Reference/README.md). |
| `Scripts/` | scripts que rodam o original e gravam `Reference/` (`export_reference.m`, `export_gpx_reference.m`, `export_import_fixtures.m`) |
| `Tools/` | para quem ainda usa MATLAB: abrir os resultados do C++ nos gráficos originais (`plotCppResult`, `loadCppResult`, `compareCppResult`, `loadRealLogFile`) |
| [`mapa_matlab_cpp.md`](mapa_matlab_cpp.md) | cada função do MATLAB e o equivalente em C++ |

## A conversão em resumo

O código C++ faz as mesmas contas na mesma ordem do MATLAB. Nas 5 pistas, os tempos de
volta, a velocidade máxima, a aceleração lateral e o consumo são **idênticos bit a bit**, e
a simulação roda cerca de 2.000 vezes mais rápido:

| Pista | Voltas | Tempo (igual nas duas versões) | MATLAB | C++ |
|---|---|---|---|---|
| Autocross | 2 | 47,492 s / 45,501 s | ~4,5 s | ~2 ms |
| Autocross 2024 V1 | 2 | 71,661 s / 70,113 s | ~3,8 s | ~2,8 ms |
| Autocross 2024 V1 (inverso) | 2 | 73,069 s / 71,668 s | ~3,6 s | ~2,6 ms |
| Skidpad | 1 | 6,240 s | ~0,4 s | ~0,08 ms |
| Aceleração | 1 | 4,250 s | ~0,3 s | ~0,14 ms |

Os comportamentos do modelo que vieram do MATLAB e ainda serão corrigidos estão em
[`docs/problemas_conhecidos.md`](../docs/problemas_conhecidos.md).

## Usando com o MATLAB

```matlab
addpath('MATLAB/Tools')
plotCppResult('Results/autox')                         % gráficos originais (plotResults)
SimOut = loadCppResult('Results/autox');               % mesmo formato do SimOut
compareCppResult('Results/autox', 'Data/Private/log_autox.csv')   % plotComparison original
```

Para regenerar a referência (só é preciso se o código em `Original/` mudar):

```bash
matlab -batch "addpath('MATLAB/Scripts'); export_reference"
```
