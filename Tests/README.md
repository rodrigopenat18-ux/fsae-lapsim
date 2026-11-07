# Tests

Testes automáticos do simulador ([doctest](https://github.com/doctest/doctest)). Rode depois
de compilar:

```bash
ctest --preset release
```

Cada arquivo `test_*.cpp` testa uma parte, de `Modules/` às voltas completas e aos arquivos
gravados. Os resultados são comparados com valores de referência guardados em
[`MATLAB/Reference`](../MATLAB/Reference), a maioria com tolerância zero. Assim, qualquer
mudança que altere um número sem querer é detectada.
