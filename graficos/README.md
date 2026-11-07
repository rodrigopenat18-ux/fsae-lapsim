# graficos

Ferramentas **opcionais** para analisar os resultados fora da interface. Elas leem a pasta
gravada por `fsae_sim run` ou pelo botão "Salvar resultados" da interface (`telemetry.csv`,
`summary.json`...).

## MATLAB (`matlab/`)

```matlab
addpath('graficos/matlab')
plotCppResult('resultados/autox')         % gráficos originais (plotResults), última volta
SimOut = loadCppResult('resultados/autox'); % mesmo formato do SimOut do runSimulation
compareCppResult('resultados/autox', 'dados/privado/log_autox.csv')   % vs. log real
```

| Arquivo | O que faz |
|---|---|
| `loadCppResult.m` | lê um resultado do C++ no formato `SimOut` do MATLAB |
| `plotCppResult.m` | abre o `plotResults.m` original com esse resultado |
| `compareCppResult.m` | compara com um log real ou do OptimumLap usando o `plotComparison.m` original |
| `loadRealLogFile.m` | versão corrigida do `loadRealLog.m` (recebe o arquivo; ignora acentos) |

Os gráficos originais vêm de [`validacao/matlab_original/`](../validacao/matlab_original).

## Python (`python/`)

```bash
python graficos/python/plot_results.py resultados/autox
python graficos/python/plot_results.py resultados/setupA --compare resultados/setupB
python graficos/python/plot_results.py resultados/autox --log dados/privado/log_autox.csv
```

Precisa de `numpy` e `matplotlib`.
