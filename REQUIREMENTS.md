# Requisitos de entorno — HULK-Compiler

Dependencias para compilar y ejecutar tests en **Windows** y **Ubuntu**.  
El proyecto separa dos niveles: **build base** (lexer → evaluador) y **codegen LLVM** (Fase 4, `make test_llvm`).

| Nivel | Targets Make | ¿Necesita LLVM? |
|-------|--------------|-----------------|
| Base | `compile`, `test_semantic`, `test_eval`, `build` | No |
| Fase 4 | `test_llvm` (smokes I0–I12) | Sí (`llvm-config`, `libLLVM`, `clang`) |

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
| **libLLVM** (14 en Windows) | Smokes y API IR | — | ✓ |
| **clang** | E2E I4+: compilar `.ll` + `runtime.c` → ejecutable | — | ✓ |

> **Nota:** el parser es **LL(1) manual**; no se usa Bison en el build habitual.

---

## Windows

Entorno probado: **PowerShell** + **MinGW** (`g++`, `mingw32-make`).  
Script de PATH: [`scripts/setup_build_env.ps1`](scripts/setup_build_env.ps1).

### 1. Build base (`make compile`)

| Componente | Cómo instalarlo | Verificación |
|------------|-----------------|--------------|
| **MinGW g++** (C++17) | [MSYS2](https://www.msys2.org/) (`pacman -S mingw-w64-x86_64-gcc`) o toolchain MinGW de [GHCup](https://www.haskell.org/ghcup/) | `g++ --version` |
| **mingw32-make** | MSYS2: `pacman -S mingw-w64-x86_64-make` | `mingw32-make --version` |
| **flex++** + **FlexLexer.h** | `winget install WinFlexBison.win_flex_bison` **o** MSYS2: `pacman -S flex` | `flex++ --version` |
| **Binutils (bfd)** | Incluido en MinGW/MSYS2; el `Makefile` usa `-fuse-ld=bfd` | `ld.bfd --version` |
| **Git** | [git-scm.com](https://git-scm.com/) o `winget install Git.Git` | `git --version` |

**Paths que el Makefile espera (ajustables):**

| Recurso | Path típico |
|---------|-------------|
| WinFlexBison | `%LOCALAPPDATA%\Microsoft\WinGet\Packages\WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe` |
| Headers MSYS2 (alternativa) | `C:\ghcup\msys64\usr\include` o `C:\msys64\usr\include` |
| Make (alternativa) | `C:\Strawberry\c\bin` (Strawberry Perl) |

**Instalación rápida (winget + MSYS2):**

```powershell
winget install Git.Git
winget install WinFlexBison.win_flex_bison
# MSYS2: instalar mingw-w64-x86_64-gcc, mingw-w64-x86_64-make
```

**Build:**

```powershell
cd HULK-Compiler
. .\scripts\setup_build_env.ps1
mingw32-make compile
mingw32-make test_semantic
mingw32-make test_eval
```

### 2. Codegen LLVM — Fase 4 (`make test_llvm`)

Además del build base:

| Componente | Cómo instalarlo | Verificación |
|------------|-----------------|--------------|
| **LLVM 14** (`llvm-config`, headers, `libLLVM`) | [GHCup](https://www.haskell.org/ghcup/): toolchain MinGW con LLVM en `C:\ghcup\ghc\<versión>\mingw\bin` | `llvm-config --version` |
| **clang** | Mismo bundle GHCup/LLVM MinGW (debe estar en `PATH` para smokes E2E I4+) | `clang --version` |

El `Makefile` asume por defecto:

```text
LLVM_CONFIG = C:/ghcup/ghc/9.6.7/mingw/bin/llvm-config
```

Si tu instalación está en otra ruta:

```powershell
mingw32-make test_llvm LLVM_CONFIG="C:/ruta/a/llvm-config"
```

**Restricción ABI (Windows):** los smokes LLVM se compilan con `-fno-exceptions` y `-fno-rtti` para coincidir con **libLLVM-14** de GHCup. No mezclar otra versión de LLVM sin revisar flags.

**Build Fase 4:**

```powershell
. .\scripts\setup_build_env.ps1
mingw32-make test_llvm
```

> `make compile` **no** enlaza libLLVM (rápido, ~40 s). Solo `test_llvm` la necesita.

---

## Ubuntu

Probado en **22.04 LTS** y **24.04 LTS** con paquetes del repositorio.

### 1. Build base

```bash
sudo apt update
sudo apt install -y \
  build-essential \
  flex \
  git \
  make
```

| Paquete | Provee |
|---------|--------|
| `build-essential` | `g++`, `make`, `ld` |
| `flex` | `flex++`, `FlexLexer.h` |
| `git` | Clonar repos |

**Build:**

```bash
cd HULK-Compiler
make compile
make test_semantic
make test_eval
```

### 2. Codegen LLVM — Fase 4

```bash
sudo apt install -y \
  llvm-14-dev \
  clang-14
```

En distros donde el paquete por defecto ya es LLVM 14+:

```bash
sudo apt install -y llvm-dev clang
```

| Paquete | Provee |
|---------|--------|
| `llvm-14-dev` | `llvm-config-14`, headers, `libLLVM-14` |
| `clang-14` | Compilar IR `.ll` → ejecutable (E2E I4+) |

Si `llvm-config` no está en el PATH, enlazar el binario de la versión 14:

```bash
sudo update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-14 100
llvm-config --version   # debe responder
```

**Build Fase 4:**

```bash
make test_llvm
# o, si hace falta:
make test_llvm LLVM_CONFIG=llvm-config-14
```

---

## Workspace (dos repos)

Además de las herramientas anteriores, hace falta **Git** para el layout documentado:

```bash
git clone 
cd HULK-Compiler
mkdir -p docs && cd docs
git clone  .
cd ..
```

| Repo | Ruta | Contenido |
|------|------|-----------|
| Código | `HULK-Compiler/` | Compilador, tests, Makefile |


---

## Verificación rápida

### Windows (PowerShell)

```powershell
g++ --version
mingw32-make --version
flex++ --version
git --version
llvm-config --version    # solo Fase 4
clang --version          # solo Fase 4 (E2E I4+)
```

### Ubuntu

```bash
g++ --version
make --version
flex++ --version
git --version
llvm-config --version    # solo Fase 4
clang --version            # solo Fase 4 (E2E I4+)
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
- [`scripts/setup_build_env.ps1`](scripts/setup_build_env.ps1) — PATH Windows
- [`Codegen/README.md`](Codegen/README.md) — estado codegen LLVM
