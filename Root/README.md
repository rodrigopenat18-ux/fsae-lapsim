# Root

Os programas, ou seja, o que você roda.

| Pasta | Programa | Para quê |
|---|---|---|
| `GUI/` | `fsae_gui.exe` | interface gráfica: editar o carro, escolher a pista, rodar e ver os gráficos |
| `CLI/` | `fsae_sim.exe` | linha de comando: rodar em lote, em scripts ou num otimizador (`fsae_sim run --track autox --json`) |
| `Python/` | `plot_results.py` | opcional: gráficos de resultados salvos e comparação com logs reais (precisa de `numpy` e `matplotlib`) |

Depois de compilar, `fsae_gui.exe` e `fsae_sim.exe` são copiados para a raiz do projeto, ao
lado de `Data/` (os originais ficam em `build/release/Root/...`). Os dois procuram a pasta
[`Data/`](../Data) ao lado do executável ou nas pastas acima, ou na variável de ambiente
`FSAE_DATA_DIR`.

Como usar cada um: seções "Interface gráfica", "Linha de comando" e "Comparar com dados
reais" do [README principal](../README.md).
