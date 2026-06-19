#!/usr/bin/env bash
# Entorno Ubuntu 22.04+ para Fase 4 (LLVM 21.1.x).
#
# Uso:
#   source scripts/setup_ubuntu_env.sh          # solo esta terminal
#   source scripts/setup_ubuntu_env.sh --persist # + guardar en ~/.bashrc
#
# Requiere: sudo apt install llvm-21-dev clang-21 (ver REQUIREMENTS.md § Ubuntu)

MARK_BEGIN="# >>> HULK-Compiler LLVM 21 >>>"
MARK_END="# <<< HULK-Compiler LLVM 21 <<<"

hulk_setup_llvm_path() {
    if command -v llvm-config-21 >/dev/null 2>&1; then
        export LLVM_CONFIG=llvm-config-21
    elif command -v llvm-config >/dev/null 2>&1 && llvm-config --version 2>/dev/null | grep -q '^21\.'; then
        export LLVM_CONFIG=llvm-config
    else
        echo "[setup_ubuntu_env] llvm-config 21 no encontrado. Instala llvm-21-dev (REQUIREMENTS.md)." >&2
        return 1
    fi

    export LLVM_PREFIX="$($LLVM_CONFIG --prefix 2>/dev/null || echo /usr/lib/llvm-21)"
    LLVM_BINDIR="$($LLVM_CONFIG --bindir 2>/dev/null || echo /usr/lib/llvm-21/bin)"
    export PATH="${LLVM_BINDIR}:/usr/bin:${PATH}"

    if ! command -v clang >/dev/null 2>&1 && command -v clang-21 >/dev/null 2>&1; then
        GCC_INC="$(g++ -print-file-name=include 2>/dev/null || true)"
        mkdir -p "${HOME}/bin"
        cat > "${HOME}/bin/clang" <<EOF
#!/bin/bash
exec clang-21 --gcc-toolchain=/usr ${GCC_INC:+-isystem "$GCC_INC"} "\$@"
EOF
        chmod +x "${HOME}/bin/clang"
        export PATH="${HOME}/bin:${PATH}"
    fi

    return 0
}

hulk_persist_bashrc() {
    local bashrc="${HOME}/.bashrc"
    if grep -q "$MARK_BEGIN" "$bashrc" 2>/dev/null; then
        echo "[setup_ubuntu_env] Bloque LLVM ya existe en $bashrc"
        return 0
    fi
    cat >>"$bashrc" <<'EOF'

# >>> HULK-Compiler LLVM 21 >>>
# clang + llvm-config-21 para ./hulk → ./output (Fase 4)
export LLVM_CONFIG=llvm-config-21
export PATH="/usr/lib/llvm-21/bin:${PATH}"
# <<< HULK-Compiler LLVM 21 <<<
EOF
    echo "[setup_ubuntu_env] Añadido a $bashrc — abre una terminal nueva o: source ~/.bashrc"
}

if [[ "${1:-}" == "--persist" ]]; then
    hulk_persist_bashrc
    if [[ -n "${BASH_SOURCE[0]:-}" && "${BASH_SOURCE[0]}" == "${0}" ]]; then
        exit 0
    fi
fi

if [[ -n "${BASH_SOURCE[0]:-}" && "${BASH_SOURCE[0]}" != "${0}" ]]; then
    if hulk_setup_llvm_path; then
        echo "[setup_ubuntu_env] LLVM=$($LLVM_CONFIG --version) clang=$(command -v clang) ($(clang --version 2>/dev/null | head -1))"
    else
        return 1
    fi
else
    echo "Uso: source scripts/setup_ubuntu_env.sh [--persist]" >&2
    exit 1
fi
