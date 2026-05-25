# HULK-Compiler

Compilador del lenguaje HULK (lexer Flex, parser LL(1), AST).

## Documentación


## Compilación rápida

```bash
make lexer    # regenerar Lexer/hulk_lexer.cpp desde Flex
make compile
make execute FILE=Parser/tests/valid_expr_pipeline.hulk
```

## Tests

```bash
./tests/run_lexer_phase.sh
./tests/run_extensions.sh
```

