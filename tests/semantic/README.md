# Tests semánticos — Fase 2

Fixtures `.hulk` para reglas R1–R4. Plan: [`docs/tests/fase2_semantic_tests.md`](../../docs/tests/fase2_semantic_tests.md). Reglas: [`docs/semantic/hulk_semantic_rules.md`](../../docs/semantic/hulk_semantic_rules.md).

## Convenciones

- **`valid/`** — parse OK; sin errores semánticos.
- **`invalid/`** — parse OK; `ErrorType` según el plan.
- Varias sentencias con `;`; `function` **solo global**, antes del uso textual.
- **No** hay funciones dentro de `let` ni closures con `function` anidada.

## Ejecución (cuando exista `--semantic`)

```bash
hulk.exe tests/semantic/valid/01_r1_shadow_nested_let.hulk --semantic
```

## Parser (no en esta carpeta)

Casos **SYN.*** en [`docs/plan_de_arreglos.md`](../../docs/plan_de_arreglos.md) (operandos de `+`, bloque como expresión, `function` dentro de `let`).
