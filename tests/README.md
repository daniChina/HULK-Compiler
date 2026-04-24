# Suite incremental de pruebas HULK

Esta carpeta organiza pruebas del lexer y reserva aparte los casos del parser.

## Estado actual

La suite activa del lexer vive dentro de:

- `tests/phase_00_lexer_errors/`

Ahí hay dos niveles:

- casos generales del lexer en la raíz de `phase_00_lexer_errors`
- fases léxicas por subconjunto en:
  - `phase_01_expressions`
  - `phase_02_control`
  - `phase_03_functions`
  - `phase_04_types`

## Convención actual

Para el lexer:

- `valid/`: el archivo debe tokenizarse sin `UNKNOWN` ni errores léxicos
- `invalid/`: el archivo debe producir un error léxico real, o documentar una mejora léxica pendiente

Los errores sintácticos se guardan aparte en:

- `tests/pending_parser/`

## Diseño recomendado del lexer

Pensando en un parser posterior inspirado en el de amabe, la recomendación es:

- `print`, `sin`, `cos`, `sqrt`, `log`, `rand`, `range`, `PI`, `E`: mejor tratarlos como identificadores o nombres globales especiales en semántica, no como tokens léxicos distintos
- `true` y `false`: mejor separarlos como tokens propios (`TRUE`, `FALSE`) en vez de colapsarlos en un solo `BOOLEAN_LITERAL`

## Uso

Correr los casos generales de lexer:

```bash
./tests/run_lexer_phase.sh phase_00_lexer_errors
```

Correr una fase léxica específica:

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
