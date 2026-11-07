# programas

| Pasta | Executável | Para quê |
|---|---|---|
| `interface/` | `fsae_gui.exe` | interface gráfica: editar o carro, escolher a pista, rodar e ver os gráficos |
| `linha_de_comando/` | `fsae_sim.exe` | rodar em lote, em scripts ou num otimizador (`fsae_sim run --track autox --json`) |

Depois de compilar, os executáveis ficam em `build/release/programas/...`. Os dois procuram a
pasta [`dados/`](../dados) subindo a partir da pasta do executável, ou na variável de
ambiente `FSAE_DATA_DIR`.

Como usar cada um: seções "Interface gráfica" e "Comandos" do [README principal](../README.md).
