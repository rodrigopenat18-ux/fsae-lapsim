# fsae-lapsim

Simulador de tempo de volta para Formula SAE em C++17 — port do simulador MATLAB
(modelo de ponto de massa com transferência de carga, aerodinâmica, powertrain com câmbio,
temperatura do disco de freio e consumo de combustível por BSFC).

> **Status:** em desenvolvimento. Fase 0 (esqueleto do projeto) concluída; a simulação ainda
> está sendo portada módulo a módulo e validada contra o MATLAB original (pasta `matlab/`).

*English summary: Formula SAE lap time simulator written in C++17, ported from a MATLAB
point-mass simulator. Work in progress.*

## Provas simuladas

- Aceleração (75 m)
- Skidpad (5 m de arrancada + 1 volta)
- Autocross (volta parada + volta lançada)
- Pista customizada

## Requisitos

- Compilador C++17 (GCC ≥ 9, Clang ≥ 10 ou MSVC 2019+)
- CMake ≥ 3.20
- Ninja (recomendado)
- Internet na primeira compilação (o CMake baixa as dependências de teste)

### Windows (MSYS2)

```bash
winget install --id MSYS2.MSYS2 -e
```

No terminal **MSYS2 UCRT64**:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
```

Adicione `C:\msys64\ucrt64\bin` ao PATH do Windows.

## Compilar e testar

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

O executável fica em `build/release/apps/fsae_sim/`.

## Estrutura

```
include/fsae/   cabeçalhos públicos da biblioteca
src/            implementação
apps/fsae_sim/  programa de linha de comando
tests/          testes unitários e validação contra o MATLAB (tests/reference/)
tools/matlab/   scripts que geram os dados de referência a partir do MATLAB
matlab/         simulador MATLAB original (referência)
data/           carros e pistas; data/private/ não vai para o Git
```

Veja também [KNOWN_ISSUES.md](KNOWN_ISSUES.md).

## Licença

MIT — veja [LICENSE](LICENSE).
