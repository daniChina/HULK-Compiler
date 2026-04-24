# Suite incremental de pruebas HULK

Esta carpeta organiza pruebas del lexer y reserva aparte los casos del parser.

## Estado actual

La suite activa del lexer vive dentro de:

- `tests/phase_00_lexer_errors/`

Ahi hay dos niveles:

- casos generales del lexer en la raiz de `phase_00_lexer_errors`
- fases lexicas por subconjunto en:
  - `phase_01_expressions`
  - `phase_02_control`
  - `phase_03_functions`
  - `phase_04_types`

## Convencion actual

Para el lexer:

- `valid/`: el archivo debe tokenizarse sin `UNKNOWN` ni errores lexicos
- `invalid/`: el archivo debe producir un error lexico real, o documentar una mejora lexica pendiente

Los errores sintacticos se guardan aparte en:

- `tests/pending_parser/`

## Contrato actual del lexer

El lexer actual ya sigue estas decisiones de diseño:

- `print`, `sin`, `cos`, `sqrt`, `log`, `rand`, `range`, `PI`, `E` se reconocen como `IDENTIFIER`
- `true` y `false` se reconocen como tokens propios `TRUE` y `FALSE`

Motivo:

- los builtins no deben introducir categorias sintacticas especiales si su forma es la misma que cualquier identificador
- `TRUE` y `FALSE` encajan mejor con un parser posterior inspirado en gramaticas LL(1) o Bison

## Uso

Correr los casos generales de lexer:

```bash
./tests/run_lexer_phase.sh phase_00_lexer_errors
```

Correr una fase lexica especifica:

```bash
./tests/run_lexer_phase.sh phase_01_expressions
./tests/run_lexer_phase.sh phase_02_control
./tests/run_lexer_phase.sh phase_03_functions
./tests/run_lexer_phase.sh phase_04_types
```

Correr todo el bloque de lexer:

```bash
./tests/run_lexer_phase.sh all
```
