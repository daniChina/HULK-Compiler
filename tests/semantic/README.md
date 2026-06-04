# Tests semánticos — Fase 2


## Convenciones

- **`valid/`** — parse OK; sin errores semánticos.
- **`invalid/`** — parse OK; `ErrorType` según el plan.
- Varias sentencias con `;`; `function` **solo global**, antes del uso textual.
- **No** hay funciones dentro de `let` ni closures con `function` anidada.

## Ejecución

```bash
make compile
make test_semantic_fixtures    # todos los .hulk en valid/ e invalid/
powershell -File scripts/run_semantic.ps1   # Windows (sin make)
bash scripts/run_semantic.sh   # Linux / Git Bash

hulk.exe tests/semantic/valid/01_r1_shadow_nested_let.hulk --semantic
make test_r1_semantic
make test_r2_semantic
make test_r3_r4_semantic
make test_a4_builtins
make test_semantic

hulk.exe tests/semantic/invalid/07_r2_free_identifier.hulk --semantic
```

**Expectativas inválidos:** `tests/semantic/manifest.tsv` (`kind`: `semantic` | `parse` | `any` + fragmento en stderr).

**Bloques / `let`:** válido `17_r1_block_nested_let_manual.hulk` / `04_r1_shadow_block_nested_let.hulk`; inválido sin `in` → `19_block_multi_let_without_in.hulk`, bloque suelto → `20_block_stmt_alone.hulk`.

## Parser (no en esta carpeta)

