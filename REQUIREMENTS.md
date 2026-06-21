# Requisitos de entorno — HULK-Compiler

Dependencias para compilar y ejecutar tests en **Windows** y **Ubuntu**.  
El proyecto separa dos niveles: **build base** (lexer → evaluador) y **codegen LLVM** (Fase 4, `make test_llvm`).

| Nivel | Targets Make | ¿Necesita LLVM? |
|-------|--------------|-----------------|
| Base | `compile`, `test_semantic`, `test_eval`, `build` | No* |
| Fase 4 | `test_llvm` (smokes I0–I12) | Sí (`llvm-config`, `libLLVM`, `clang`) |

\* **`make compile`** enlaza codegen automáticamente si `llvm-config` 21.1.x está en PATH (`-DHULK_HAVE_LLVM`). Sin LLVM: solo intérprete. Forzar sin LLVM: `make compile_nollvm`.

---

## Resumen por herramienta

| Herramienta | Uso | Base | Fase 4 |
|-------------|-----|:----:|:------:|
| **g++** (C++17) | Compilar compilador y smokes | ✓ | ✓ |
| **make** / **mingw32-make** | Orquestar build | ✓ | ✓ |
| **flex++** | Regenerar `Lexer/hulk_lexer.cpp` (`make lexer`) | ✓ | ✓ |
| **FlexLexer.h** | Cabecera del lexer C++ | ✓ | ✓ |
| **Git** | Clonar repos (código + docs) | ✓ | ✓ |
| **llvm-config** | Flags/enlace contra libLLVM | — | ✓ |
| **libLLVM** (21.1.x) | Smokes y API IR | — | ✓ |
| **clang** | E2E I4+: compilar `.ll` + `runtime.c` → ejecutable | — | ✓ |

> **Nota:** el parser es **LL(1) manual**; no se usa Bison en el build habitual.


---

## Cuándo verificar el entorno

No hace falta reinstalar nada en cada sesión. Verifica **solo cuando**:

| Situación | Nivel mínimo | Comandos de confirmación |
|-----------|--------------|---------------------------|
| Clonaste el repo o cambiaste de máquina | Base + (Fase 4 si vas a codegen) | Verificación completa abajo |
| `make compile` / `mingw32-make compile` falla | Base | `g++`, `make`, `FlexLexer.h` |
| `make lexer` falla | Base + flex | `flex++ --version` |
| `make test_semantic` o `test_eval` fallan tras compilar bien | Base (ya OK si compile pasó) | Repetir esos targets |
| `make test_llvm` falla al **compilar** smokes | Fase 4 (LLVM 21) | `llvm-config --version`, `where`/`which` |
| `make test_llvm` compila pero un smoke **E2E** falla (I4+) | Fase 4 + `clang` en PATH | `clang --version` |
| Cambiaste PATH, MSYS2, apt o toolchain | Todo lo que uses | Verificación completa |

**Regla:** `g++`, `clang` y `llvm-config` deben venir del **mismo** entorno (UCRT64 en Windows; apt.llvm.org en Ubuntu). Mezclar GHCup LLVM 14, MSVC llvm.org o dos `llvm-config` en PATH rompe el build.

---

## Verificación e instalación — Windows

Entorno probado: **PowerShell** + **MSYS2 UCRT64** (`g++`, `mingw32-make`, LLVM 21).  
Script de PATH: [`scripts/setup_build_env.ps1`](scripts/setup_build_env.ps1).

### 1. Verificación rápida (build base)

Desde la raíz del repo:

```powershell
cd HULK-Compiler
. .\scripts\setup_build_env.ps1
```

Comprueba manualmente o con el script:

```powershell
.\scripts\check_toolchain.ps1   # falla si falta algo obligatorio para Fase 4
# Para solo base (sin LLVM), basta con:
g++ --version
mingw32-make --version
git --version
Test-Path "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe\FlexLexer.h"
```

> **Nota:** `check_toolchain.ps1` exige `flex++` y LLVM 21 (entorno completo). Para solo `make compile`, basta `g++`, `mingw32-make` y `FlexLexer.h`; `flex++` solo hace falta para `make lexer`.

| Herramienta | Esperado | Si falla |
|-------------|----------|----------|
| `g++` | MinGW, C++17 (ideal: MSYS2 UCRT64 15.x) | Instalar MSYS2 + `pacman -S mingw-w64-ucrt-x86_64-gcc` |
| `mingw32-make` | GNU Make 4.x | `pacman -S mingw-w64-ucrt-x86_64-make` |
| `FlexLexer.h` | Existe en WinFlexBison o `C:\msys64\usr\include` | `winget install WinFlexBison.win_flex_bison` o `pacman -S flex` (shell MSYS2) |
| `flex++` | Solo si regeneras lexer (`make lexer`) | WinFlexBison o `pacman -S flex` en UCRT64 |
| `git` | Cualquier versión reciente | `winget install Git.Git` |

**Confirmación base:**

```powershell
. .\scripts\setup_build_env.ps1
mingw32-make compile
mingw32-make test_semantic
mingw32-make test_eval
```

Si los tres pasan, el entorno base está listo.

### 2. Verificación Fase 4 (`make test_llvm`)

Requiere todo lo anterior **más** LLVM 21 del mismo prefix UCRT64:

```powershell
. .\scripts\setup_build_env.ps1
llvm-config --version    # debe ser 21.1.x
where.exe llvm-config  # un solo resultado, p. ej. C:\msys64\ucrt64\bin\llvm-config.exe
clang --version        # 21.x, mismo prefix que llvm-config
.\scripts\check_toolchain.ps1
mingw32-make test_llvm
```

| Herramienta | Esperado | Si falla |
|-------------|----------|----------|
| `llvm-config` | `21.1.x`, en `ucrt64\bin` | Ver instalación abajo |
| `clang` | 21.x, mismo `ucrt64\bin` | `pacman -S mingw-w64-ucrt-x86_64-clang` |
| Un solo LLVM en PATH | `where llvm-config` → 1 ruta | Quitar GHCup/llvm.org del PATH |

### 3. Instalación desde cero (Windows)

**Prerrequisito:** [MSYS2](https://www.msys2.org/) instalado (p. ej. `C:\msys64`).

Abre la shell **UCRT64** de MSYS2 (no MINGW64) y ejecuta:

```bash
pacman -Syu
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make \
           mingw-w64-ucrt-x86_64-llvm mingw-w64-ucrt-x86_64-clang
```

En **PowerShell** (repo):

```powershell
winget install Git.Git
winget install WinFlexBison.win_flex_bison   # FlexLexer.h para el lexer
cd HULK-Compiler
. .\scripts\setup_build_env.ps1
.\scripts\check_toolchain.ps1
mingw32-make compile
mingw32-make test_llvm    # cuando Fase C (API LLVM) esté cerrada
```

**Paths que el Makefile usa:**

| Recurso | Path típico |
|---------|-------------|
| UCRT64 bin (gcc, llvm, clang, make) | `C:\msys64\ucrt64\bin` |
| Headers flex (alternativa) | `C:\msys64\usr\include` |
| WinFlexBison | `%LOCALAPPDATA%\Microsoft\WinGet\Packages\WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe` |
| Make (alternativa) | `C:\Strawberry\c\bin` |

Override manual de LLVM:

```powershell
mingw32-make test_llvm LLVM_CONFIG="C:/msys64/ucrt64/bin/llvm-config"
```

> `make compile` enlaza libLLVM **automáticamente** cuando `llvm-config` está en PATH (~varios minutos). Sin LLVM: `compile_nollvm` (~40 s). Operaciones largas muestran contador en `scripts/run_with_timer.sh`.

**Prohibido en Windows:** GHCup LLVM 14, instalador `LLVM-*-win64.exe` (MSVC), mezclar dos `llvm-config` en PATH.

---

## Verificación e instalación — Ubuntu

Probado en **22.04 LTS** y **24.04 LTS**.

### 1. Verificación rápida (build base)

```bash
cd HULK-Compiler
for cmd in g++ make flex++ git; do
  printf '%-10s ' "$cmd:"; command -v "$cmd" || echo 'FALTA'
done
test -f /usr/include/FlexLexer.h && echo 'FlexLexer.h: OK' || echo 'FlexLexer.h: FALTA'
```

**Confirmación base:**

```bash
make compile
make test_semantic
make test_eval
```

| Herramienta | Si falta |
|-------------|----------|
| `g++`, `make` | `sudo apt install -y build-essential` |
| `flex++`, `FlexLexer.h` | `sudo apt install -y flex` |
| `git` | `sudo apt install -y git` |

### 2. Verificación Fase 4 (`make test_llvm`)

```bash
llvm-config --version          # debe ser 21.1.x
which -a llvm-config           # preferir un solo binario en /usr/bin
clang --version                # 21.x
make test_llvm
```

| Comprobación | Esperado | Si falla |
|--------------|----------|----------|
| `llvm-config --version` | `21.1.x` | Instalar llvm-21 (abajo) |
| `which llvm-config` | `/usr/bin/llvm-config` → config-21 | `update-alternatives` (abajo) |
| `clang --version` | clang 21 | `sudo apt install -y clang-21` |
| E2E I4+ (clang en runtime) | smoke compila `.ll` | `clang-21` en PATH |

### 3. Instalación desde cero (Ubuntu)

**Build base:**

```bash
sudo apt update
sudo apt install -y build-essential flex git make
```

**LLVM 21 (Fase 4):**

```bash
wget https://apt.llvm.org/llvm.sh && chmod +x llvm.sh
sudo ./llvm.sh 21
sudo apt install -y llvm-21-dev clang-21 lld-21
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-21 100
llvm-config --version   # debe ser 21.1.x
```

**Confirmación completa:**

```bash
make compile && make test_semantic && make test_eval
make test_llvm    # cuando Fase C (API LLVM) esté cerrada
```

| Paquete | Provee |
|---------|--------|
| `build-essential` | `g++`, `make`, `ld` |
| `flex` | `flex++`, `FlexLexer.h` |
| `llvm-21-dev` | `llvm-config-21`, headers, `libLLVM-21` |
| `clang-21` | Compilar IR `.ll` → ejecutable (E2E I4+) |
| `lld-21` | Enlace (opcional) |

Override manual:

```bash
make test_llvm LLVM_CONFIG=llvm-config-21
```

---

## Workspace (documentación local)


| Ruta | Contenido |
|------|-----------|

---

## Verificación rápida (cheatsheet)

### Windows (PowerShell)

```powershell
. .\scripts\setup_build_env.ps1
.\scripts\check_toolchain.ps1    # base + Fase 4
mingw32-make compile && mingw32-make test_semantic && mingw32-make test_eval
```

### Ubuntu

```bash
command -v g++ make flex++ git llvm-config clang
make compile && make test_semantic && make test_eval
make test_llvm                   # Fase 4 (tras migración API)
```

### Tests por objetivo

| Objetivo | Comando |
|----------|---------|
| Compilador + intérprete | `make compile` + `make test_semantic` + `make test_eval` |
| Codegen LLVM (I0–I12) | `make test_llvm` |
| Contrato matcom (`./hulk` → `./output`) | Requiere Fase 4 **I12** + LLVM en el build principal |

---

## Referencias

- [`README.md`](README.md) — uso diario y scripts `run_hulk`
- [`scripts/setup_build_env.ps1`](scripts/setup_build_env.ps1) — PATH Windows (UCRT64 + LLVM 21)
- [`scripts/check_toolchain.ps1`](scripts/check_toolchain.ps1) — verificación Gate A (opcional)
