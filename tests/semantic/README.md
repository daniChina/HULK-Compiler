# Tests semánticos — Fase 2

Fixtures `.hulk` para reglas R1–R4. Convenciones y expectativas en `manifest.tsv` y este README.

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

**Errores de parse (jun 2026):** el driver emite `(line,col) SYNTACTIC: …` además del legacy `Error de parseo`. Los runners (`run_semantic.ps1` / `.sh`) aceptan ambos.

**Bloques / `let` (M1 matcom):**

| Fixture | Fuente | Qué prueba |
|---------|--------|------------|
| `19_block_multi_let_without_in` | `let outer = 1 in { let x = 1; let x = 2; x };` | Dos `let` en bloque **sin** `in` entre bindings → parse `BindingTail` |
| `20_block_stmt_alone` | `{ let x = 1; let x = 2; x };` | Bloque suelto con lets encadenados sin `in` → parse `BindingTail` (no confundir con `{ let v in …; … }` top-level válido) |
| `46_e_is_undefined_type` | `let x = 1 in x is Ghost;` | Tras M1, `is` ya no es keyword de herencia; la línea es **sintaxis inválida** → parse `PostfixTail` (no error semántico de tipo) |

Válidos con bloque anidado correcto: `17_r1_block_nested_let_manual.hulk`, `04_r1_shadow_block_nested_let.hulk`.

## Parser (no en esta carpeta)

Casos **SYN.1–SYN.4** cerrados; ver fixtures `invalid/` con prefijo `syn-` en `tests/semantic/` y `playground/notas_tests/`.
