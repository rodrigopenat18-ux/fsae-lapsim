# Como contribuir

Contribuições de outras equipes são bem-vindas: correções, novas pistas, melhorias de
física, ferramentas de análise.

## Regra principal: resultados não mudam sem querer

Os testes em [`Tests/`](Tests) comparam o simulador com resultados de referência, a maioria
com tolerância zero. Por isso:

- **Mudanças que não deveriam alterar resultados** (refatoração, desempenho, interface,
  entrada/saída) precisam manter todos os testes passando **sem afrouxar tolerâncias**.
- **Mudanças de física** (corrigir um item dos [problemas conhecidos](docs/problemas_conhecidos.md),
  modelar algo novo) entram como **opção desligada por padrão**. Assim, o comportamento
  atual continua disponível e testado. Junto com a mudança, mostre o impacto no tempo de
  volta das pistas de `Data/Tracks/`.

## Fluxo

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Para depurar, use o preset `debug` (ativa os `assert`s internos).

## Estilo

- C++17, sem dependências além das já usadas.
- Formatação: `.clang-format` do repositório.
- Ao alterar uma conta existente, mantenha a **mesma ordem de operações**: mudar a ordem
  muda o arredondamento e os testes acusam.
- Mensagens para o usuário em português; nomes no código em inglês.

## Dados

Não envie dados privados da equipe (logs de aquisição, GPX, exportações do OptimumLap).
A pasta `Data/Private/` é ignorada pelo Git justamente para isso.

## Sobre o MATLAB

A pasta [`MATLAB/`](MATLAB) guarda a versão original do simulador e as referências usadas
pelos testes. Só é preciso mexer lá se a referência tiver que ser regenerada
(`matlab -batch "addpath('MATLAB/Scripts'); export_reference"`).
