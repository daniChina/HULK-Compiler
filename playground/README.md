# Playground

Carpeta para probar programas HULK a mano.

## Ejecutar

Desde la **raiz del repo**:

```powershell
# Ejecutar (semantico + evaluador)
.\scripts\run_hulk.ps1 playground\hello.hulk

# Ver todo el pipeline: lexer -> parser -> semantico -> evaluador
.\scripts\run_hulk.ps1 -Pipeline playground\hello.hulk

# Una sola fase
.\scripts\run_hulk.ps1 -Lexer playground\hello.hulk
.\scripts\run_hulk.ps1 -Parse playground\hello.hulk
.\scripts\run_hulk.ps1 -SemanticOnly playground\hello.hulk
```

Instrucciones completas: [`README.md`](../README.md).

## Fixtures de notas (`notas_tests.md`)

Casos catalogados en [`notas_tests.md`](notas_tests.md) → archivos en [`notas_tests/`](notas_tests/).

```bash
bash playground/verify_notas_tests.sh   # 93 casos valid/ + invalid/
```
