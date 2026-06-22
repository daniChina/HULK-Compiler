# Fixtures de `notas_tests.md`

Casos `.hulk` derivados de las preguntas y tablas de `playground/notas_tests.md`.

## Estructura

| Carpeta | Contenido |
|---------|-----------|
| `valid/` | Deben compilar sin error (`exit 0`) |
| `invalid/` | Deben fallar con diagnóstico léxico, sintáctico o semántico |
| `manifest.tsv` | Id, ruta, kind, exit esperado y fragmento de mensaje |

Prefijos: `pos-`, `num-`, `bool-`, `mix-`, `cmt-`, `char-`, `blk-`, `r1-`…`r4-`, `inf-`, `type-`, `glob-`, `inh-`, `new-`, `func-`, `chain-`, `print-nest-`, `syn-` (mensajes sintácticos humanizados).

## Ejecución

```bash
make build
bash playground/verify_notas_tests.sh
```

## Notas de adaptación a la gramática actual

- Operadores booleanos: `and`, `or`, `!` (no `&&` / `||`).
- `BlockExpr` en `let x = {…} in …` no es válido; usar bloque tras `in` (`blk-02` vs `blk-02b`).
- `r4-03` y `r4-06`: referencia adelantada y recursión mutua entre funciones globales **sí compilan** en este proyecto.
- `num-05`: `@` exige operandos `String`; el caso ` @ 42` del apunte se dejó como concat `"42"`.
- `pos-08`: sin arrays en el lexer; se usa `sin("a")` como error de tipo en argumento.

Regenerar `manifest.tsv` tras cambios en el compilador:

```bash
python3 scripts/gen_notas_manifest.py
```
