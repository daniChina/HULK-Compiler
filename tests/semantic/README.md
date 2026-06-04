# Tests semánticos — Fase 2


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

