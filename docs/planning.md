# Plan incremental para igualar el subconjunto de HULK del proyecto de amabe

## Objetivo

El objetivo del proyecto no es implementar todo HULK de una vez, sino avanzar por subconjuntos verificables hasta poder procesar el mismo lenguaje que maneja actualmente el parser de referencia de amabe.

La referencia principal para el alcance sintáctico será el parser de amabe en dos archivos:

- `Parser/grammar.ll1`
- `Parser/parser.y`

## Decisiones de diseño léxico recomendadas

Para no acoplar innecesariamente la sintaxis a la biblioteca estándar, y para facilitar un parser posterior inspirado en amabe, se recomienda:

- tratar `print`, `sin`, `cos`, `sqrt`, `exp`, `log`, `rand`, `range`, `PI`, `E` como identificadores o nombres globales especiales, no como tokens léxicos separados
- tratar `true` y `false` como tokens propios (`TRUE`, `FALSE`), no como un único `BOOLEAN_LITERAL`

## Separación actual de pruebas

- `tests/phase_00_lexer_errors/`: suite activa del lexer
- `tests/phase_00_lexer_errors/phase_01_expressions/`: expresiones y operadores a nivel léxico
- `tests/phase_00_lexer_errors/phase_02_control/`: control de flujo a nivel léxico
- `tests/phase_00_lexer_errors/phase_03_functions/`: funciones a nivel léxico
- `tests/phase_00_lexer_errors/phase_04_types/`: tipos y OO básica a nivel léxico
- `tests/pending_parser/`: casos sintácticos reservados para cuando exista parser

## Regla de trabajo actual

Antes de seguir con parser, cada fase léxica debe cumplir:

1. todos los `valid/` se tokenizan sin errores léxicos
2. los `invalid/` producen errores léxicos reales o dejan documentada una mejora pendiente del lexer
3. el runner `tests/run_lexer_phase.sh` puede ejecutar cada fase o todas juntas
