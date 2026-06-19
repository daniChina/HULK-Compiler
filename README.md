# HULK-Compiler

Compilador del lenguaje **HULK** (lexer, parser LL(1), analisis semantico, interprete).  

---



##Correr mi propio codigo HULK
cd ~/Documents/3er\ Ano/2do\ SEMESTRE/COMPILACION/compiler/HULK-Compiler
export PATH=/usr/lib/llvm-21/bin:$PATH

nano playground/mi_programa.hulk
# (pega el ejemplo greet + print)

./hulk playground/mi_programa.hulk --interpret    # ver salida rápido
./hulk playground/mi_programa.hulk && ./output    # compilar y ejecutar nativo

## Ejecutar un programa `.hulk`


### 1. Compilar el compilador (una vez)

**Ubuntu (LLVM 21 — Fase 4):**

```bash
cd ~/Documents/3er\ Ano/2do\ SEMESTRE/COMPILACION/compiler/HULK-Compiler
# o: cd ~/HULK-Compiler   (symlink sin espacios, opcional)

export LLVM_CONFIG=llvm-config-21
export PATH=/usr/lib/llvm-21/bin:$PATH
# opcional permanente: source scripts/setup_ubuntu_env.sh --persist
# opcional esta sesión: source scripts/setup_ubuntu_env.sh

make compile    # ~1–2 min → hulk_c.exe
make build      # → ./hulk (Linux: compila con -O2)
```

Sin `clang` en PATH, `./hulk` falla al generar `./output` (`clang no está en PATH`). Tras `apt install clang-21`, añade `/usr/lib/llvm-21/bin` al PATH como arriba.

**Windows (MSYS2 UCRT64):**

```bash
make compile    # → hulk_c.exe
make build      # copia a ./hulk
```


### 2. Escribir tu código HULK

Crea un archivo en **`playground/`** (o cualquier ruta):

```bash
nano playground/mi_programa.hulk
```

Ejemplo:

```hulk
function greet(name: String): String {
    "Hola, " @ name @ "!";
}

print(greet("mundo"));
```

También hay ejemplo en `playground/hello.hulk`.

### 3. Modos de uso (Linux)

| Objetivo | Comando | Qué hace |
|----------|---------|----------|
| **Ver errores** (lex / sintaxis / semántica) | `./hulk mi_programa.hulk` | Modo matcom: compila a `./output` o muestra errores en stderr |
| **Ejecutar rápido** (sin binario nativo) | `./hulk mi_programa.hulk --interpret` | Intérprete: imprime la salida en stdout |
| **Solo semántica** | `./hulk mi_programa.hulk --semantic` | `Semantic OK` o lista de errores |
| **Depuración** (tokens / AST) | `./hulk mi_programa.hulk --tokens --ast` | Modo desarrollo |

**Script cómodo** (desde la raíz):

```bash
./scripts/run_hulk.sh playground/mi_programa.hulk              # intérprete
./scripts/run_hulk.sh --semantic-only playground/mi_programa.hulk
./scripts/run_hulk.sh --pipeline playground/mi_programa.hulk   # lexer → parser → sem → eval
```

**Windows (PowerShell):**

```powershell
.\scripts\run_hulk.ps1 playground\hello.hulk
.\scripts\run_hulk.ps1 -Pipeline playground\hello.hulk
```

**Make (intérprete):**

```bash
make run FILE=playground/mi_programa.hulk
make run-pipeline FILE=playground/hello.hulk
```

### 4. Código válido → binario nativo `./output`

```bash
./hulk playground/mi_programa.hulk
```

- **Exit 0** → se generó `./output` (ejecutable Linux x86_64).
- **Exit 1 / 2 / 3** → error en stderr, formato matcom:

```
(line,col) LEXICAL: ...
(line,col) SYNTACTIC: ...
(line,col) SEMANTIC: ...
```

Ejemplo de error léxico:

```bash
printf 'let x = $bad in print(x);\n' > /tmp/malo.hulk
./hulk /tmp/malo.hulk
# stderr: (1,9) LEXICAL: unexpected character '$'
# exit: 1
```

### 5. Ver la salida del programa compilado

```bash
./hulk playground/mi_programa.hulk   # genera ./output si no hay errores
./output                             # stdout del programa
echo $?                              # exit del programa (no del compilador)
```

Comparar intérprete vs binario (deben coincidir):

```bash
./hulk playground/mi_programa.hulk --interpret
./hulk playground/mi_programa.hulk && ./output
```

### 6. Qué es `./output`

`./hulk` **no ejecuta** el programa directamente: genera un ejecutable nativo `./output` en el directorio actual.

Pipeline interno:

```
tu.hulk → análisis → LLVM IR (.hulk_out.ll) → clang + Codegen/runtime.c → ./output
```

| Archivo | Rol |
|---------|-----|
| `./output` | Ejecutable final (ELF 64-bit en Linux) |
| `.hulk_out.ll` | IR LLVM (depuración) |

Qué puedes hacer con `./output`: ejecutarlo (`./output`), depurarlo (`gdb ./output`), copiarlo a otra máquina Linux x86_64.

### 7. Resumen rápido (Ubuntu)

```bash
cd ~/Documents/3er\ Ano/2do\ SEMESTRE/COMPILACION/compiler/HULK-Compiler
export LLVM_CONFIG=llvm-config-21
export PATH=/usr/lib/llvm-21/bin:$PATH

make build

# Errores de compilación
./hulk playground/mi_programa.hulk 2>&1

# Ejecutar rápido (intérprete)
./hulk playground/mi_programa.hulk --interpret

# Compilar y ver salida nativa
./hulk playground/mi_programa.hulk && ./output
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

Guía completa (Windows + Ubuntu, base y Fase 4 LLVM): [`REQUIREMENTS.md`](REQUIREMENTS.md).

Resumen mínimo:

- C++17 (`g++`)
- Flex++ (`flex++` → `Lexer/hulk_lexer.cpp`; ver `make lexer`)
- Make (`mingw32-make` en Windows MinGW)
- Fase 4 (`make test_llvm`): LLVM **21.1.x** (`llvm-config-21`), `clang-21` en PATH — ver [`REQUIREMENTS.md`](REQUIREMENTS.md) § Ubuntu
