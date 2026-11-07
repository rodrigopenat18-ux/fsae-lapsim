# validacao

Tudo o que garante que o simulador em C++ dá **o mesmo resultado** do simulador MATLAB.

| Pasta | O que tem |
|---|---|
| `matlab_original/` | o simulador MATLAB original, **sem modificações**: é a referência |
| `gerar_gabarito/` | scripts MATLAB que rodam o original e gravam as saídas (`export_reference.m` e afins) |
| `gabarito/` | as saídas gravadas pelo MATLAB: cada função, 10 retas e 5 pistas completas ([detalhes](gabarito/README.md)) |
| `testes/` | os testes em C++, que comparam o simulador com o gabarito |

Para rodar os testes, depois de compilar:

```bash
ctest --preset release
```

Para regenerar o gabarito (precisa do MATLAB):

```bash
matlab -batch "addpath('validacao/gerar_gabarito'); export_reference"
```

Quase tudo bate **bit a bit** (tolerância zero). As poucas exceções, todas de 1 bit e sem
efeito no tempo de volta, estão explicadas em [`gabarito/README.md`](gabarito/README.md).
