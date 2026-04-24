# Plan incremental para igualar el subconjunto de HULK del proyecto de amabe

## Objetivo

El objetivo del proyecto no es implementar todo HULK de una vez, sino avanzar por subconjuntos verificables hasta poder procesar el mismo lenguaje que maneja actualmente el parser de referencia de amabe.

La referencia principal para el alcance sintactico sera el parser de amabe en dos archivos:

- `Parser/grammar.ll1`
- `Parser/parser.y`

## Decisiones lexicas activas

Para no acoplar innecesariamente la sintaxis a la biblioteca estandar, y para facilitar un parser posterior inspirado en amabe, el lexer actual sigue estas reglas:

- `print`, `sin`, `cos`, `sqrt`, `exp`, `log`, `rand`, `range`, `PI`, `E` se reconocen como `IDENTIFIER`
- `true` y `false` se reconocen como tokens propios `TRUE` y `FALSE`

Motivo:

- los builtins son una decision semantica, no una categoria sintactica obligatoria
- `TRUE` y `FALSE` explicitos simplifican el contrato con gramaticas LL(1) o Bison

## Separacion actual de pruebas

- `tests/phase_00_lexer_errors/`: suite activa del lexer
- `tests/phase_00_lexer_errors/phase_01_expressions/`: expresiones y operadores a nivel lexico
- `tests/phase_00_lexer_errors/phase_02_control/`: control de flujo a nivel lexico
- `tests/phase_00_lexer_errors/phase_03_functions/`: funciones a nivel lexico
- `tests/phase_00_lexer_errors/phase_04_types/`: tipos y OO basica a nivel lexico
- `tests/pending_parser/`: casos sintacticos reservados para cuando exista parser

## Regla de trabajo actual

Antes de seguir con parser, cada fase lexica debe cumplir:

1. todos los `valid/` se tokenizan sin errores lexicos
2. los `invalid/` producen errores lexicos reales o dejan documentada una mejora pendiente del lexer
3. el runner `tests/run_lexer_phase.sh` puede ejecutar cada fase o todas juntas
