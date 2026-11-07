# Como contribuir

Contribuições de outras equipes são bem-vindas: correções, novas pistas, melhorias de
física, ferramentas de análise.

## Regra principal: o MATLAB é o gabarito

Esta versão reproduz o simulador MATLAB original bit a bit, e os testes garantem isso.
Por isso:

- **Mudanças que não deveriam alterar resultados** (refatoração, desempenho, entrada/saída)
  precisam manter todos os testes passando **sem afrouxar tolerâncias**.
- **Mudanças de física** (corrigir um item do [KNOWN_ISSUES.md](KNOWN_ISSUES.md), modelar
  algo novo) entram como **opção desligada por padrão**. Assim, o comportamento original
  continua disponível e validado. Junto com a mudança, mostre o impacto no tempo de volta
  das pistas de `data/tracks/`.

## Fluxo

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Para depurar, use o preset `debug` (ativa os `assert`s internos).

Se mudar algo no `matlab/` ou no exportador, regenere as referências e explique a mudança
no PR:

```bash
matlab -batch "addpath('tools/matlab'); export_reference"
```

## Estilo

- C++17, sem dependências além das já usadas (nlohmann/json e doctest).
- Formatação: `.clang-format` do repositório.
- Ao portar ou alterar uma conta, mantenha a **mesma ordem de operações** do MATLAB e
  cite a função original no comentário. A ordem muda o arredondamento e quebra a
  comparação bit a bit.
- Mensagens para o usuário em português; nomes no código em inglês, com o nome MATLAB
  correspondente entre parênteses na documentação.

## Dados

Não envie dados privados da equipe (logs de aquisição, GPX, exportações do OptimumLap).
A pasta `data/private/` é ignorada pelo Git justamente para isso.
