# HULK-Compiler

Compilador del lenguaje **HULK** (lexer, parser LL(1), analisis semantico, interprete).  
En desarrollo: codegen LLVM y alineacion con el CI de [matcom/compilers](https://github.com/matcom/compilers).

---

## Ejecutar un programa `.hulk`

### 1. Compilar el compilador (una vez)

Desde la raiz del repo, con `g++`, `flex++` y `make` disponibles (MSYS2 en Windows):

```bash
make compile
```

Genera `hulk_c.exe` en la raiz.

### 2. Escribir codigo

Crea un archivo `.hulk` en la carpeta **`playground/`** (o en cualquier ruta). Ejemplo incluido: `playground/hello.hulk`.

### 3. Ejecutarlo

Por defecto corre **semantica + evaluador** y muestra la salida o los errores.

**Windows (PowerShell)** — desde la raiz del repo:

```powershell
.\scripts\run_hulk.ps1 playground\hello.hulk
```

Sin argumentos usa el ejemplo por defecto:

```powershell
.\scripts\run_hulk.ps1
```

**Linux / macOS / MSYS:**

```bash
chmod +x scripts/run_hulk.sh
./scripts/run_hulk.sh playground/hello.hulk
```

O con Make:

```bash
make run                           # playground/hello.hulk
make run FILE=playground/mi_app.hulk
make run-pipeline FILE=playground/hello.hulk
```

---

## Revisar el pipeline del compilador

El compilador tiene cuatro fases implementadas. Puedes inspeccionar cada una por separado o verlas **todas en orden** con `-Pipeline`:

| Fase | Que hace | PowerShell | Bash |
|------|----------|------------|------|
| **0 — Lexer** | Tokeniza el fuente | `.\scripts\run_hulk.ps1 -Lexer archivo.hulk` | `./scripts/run_hulk.sh --lexer archivo.hulk` |
| **1 — Parser** | CST + AST (LL(1)) | `.\scripts\run_hulk.ps1 -Parse archivo.hulk` | `./scripts/run_hulk.sh --parse archivo.hulk` |
| **2 — Semantico** | Tipos, alcance, clases | `.\scripts\run_hulk.ps1 -SemanticOnly archivo.hulk` | `./scripts/run_hulk.sh --semantic-only archivo.hulk` |
| **3 — Evaluador** | Ejecuta el programa | `.\scripts\run_hulk.ps1 archivo.hulk` | `./scripts/run_hulk.sh archivo.hulk` |
| **Pipeline completo** | Las 4 fases seguidas | `.\scripts\run_hulk.ps1 -Pipeline archivo.hulk` | `./scripts/run_hulk.sh --pipeline archivo.hulk` |

Ejemplo — recorrer todo el proceso de `playground/hello.hulk`:

```powershell
.\scripts\run_hulk.ps1 -Pipeline playground\hello.hulk
```

Veras, en orden:

1. **Tokens** — salida del lexer (`NUMBER`, `STRING`, `IDENTIFIER`, …)
2. **CST / AST** — arbol de parseo y AST
3. **Resultado semantico** — `Semantic OK` o lista de errores
4. **Salida del programa** — lo que imprime `print(...)`

Si una fase falla en `-Pipeline`, se detiene ahi y muestra el error (lexico, sintactico o semantico).

Flags utiles en parser: `-NoAst` / `-NoCst` (PowerShell) u `--no-ast` / `--no-cst` (bash) para acortar la salida.

### Invocacion directa del binario

Siempre **desde la raiz del repo** (la gramatica es una ruta relativa):

```powershell
# Lexer
.\hulk_c.exe playground\hello.hulk --tokens

# Parser
.\hulk_c.exe playground\hello.hulk --cst --ast

# Semantico
.\hulk_c.exe playground\hello.hulk --semantic

# Evaluador
.\hulk_c.exe playground\hello.hulk --semantic --interpret
```

### Que veras en consola

- **`-- Salida del programa --`**: lo que imprime tu codigo (`print(...)`, etc.).
- **`-- Errores --`**: mensajes de error (lexer, parser o semantica). Puede haber **varios** errores semanticos en una misma ejecucion.
- **`OK (exit 0)`** o **`FALLO (exit N)`** al final.

Ejemplo con el hello incluido:

```
== HULK ==
Archivo: ...\playground\hello.hulk
Modo:    semantic + interpret

-- Salida del programa --
Hola, HULK!

OK (exit 0)
```

---

## Tests del proyecto

```bash
make test_semantic          # smokes semanticos
make test_eval              # smokes del interprete
make test_semantic_fixtures # todos los .hulk en tests/semantic/
make test_eval_fixtures     # todos los .hulk en tests/eval/
```

---

## Documentacion de planificacion

- [`playground/README.md`](playground/README.md) — atajo para probar programas

---

## Requisitos de build

- C++17 (`g++`)
- Flex++ (`flex++` → `Lexer/hulk_lexer.cpp`; ver `make lexer`)
- Make

En Windows, paths tipicos: MSYS2 (`C:\ghcup\msys64\...`) o WinFlexBison via winget (ver comentarios en el `Makefile`).
