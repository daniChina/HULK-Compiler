#!/usr/bin/env bash
# Build + run smokes I0–I8 (invocado desde Makefile con run_with_timer.sh).
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
export PATH="$ROOT:$PATH"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:?CXXFLAGS requerido}"
LLVM_CXXFLAGS_ALL="${LLVM_CXXFLAGS_ALL:?LLVM_CXXFLAGS_ALL requerido}"
LLVM_LDFLAGS="${LLVM_LDFLAGS:?LLVM_LDFLAGS requerido}"

build_smoke() {
    local step="$1"
    local total="$2"
    local sources="$3"
    local target="$4"
    printf '[test_llvm] paso %s/%s: compilar %s\n' "$step" "$total" "$target" >&2
    # shellcheck disable=SC2086
    $CXX $CXXFLAGS $LLVM_CXXFLAGS_ALL $sources -o "$target" $LLVM_LDFLAGS
}

run_smoke() {
    local step="$1"
    local total="$2"
    local target="$3"
    printf '[test_llvm] paso %s/%s: ejecutar %s\n' "$step" "$total" "$target" >&2
    "./${target}"
}

TOTAL=24
N=0

next_step() { N=$((N + 1)); echo "$N"; }

# Rutas relativas (ya estamos en $ROOT): evita rotura si el path del repo tiene espacios.
S="Codegen/llvm_codegen.cpp Codegen/llvm_aux.cpp Codegen/output_build.cpp Parser/ast/expr.cpp"
COMMON="$S"

build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i0_module_smoke.cpp $COMMON" "llvm_i0_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i1_literals_smoke.cpp $COMMON" "llvm_i1_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i2_let_smoke.cpp $COMMON" "llvm_i2_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i3_binary_smoke.cpp $COMMON" "llvm_i3_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i4_print_smoke.cpp $COMMON" "llvm_i4_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i5_control_flow_smoke.cpp $COMMON" "llvm_i5_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i6_assign_smoke.cpp $COMMON" "llvm_i6_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i7_functions_smoke.cpp $COMMON" "llvm_i7_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i8_builtins_smoke.cpp $COMMON" "llvm_i8_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i9_oo_smoke.cpp $COMMON" "llvm_i9_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i10_inherit_smoke.cpp $COMMON" "llvm_i10_smoke"
build_smoke "$(next_step)" "$TOTAL" "Codegen/tests/i11_is_as_smoke.cpp $COMMON" "llvm_i11_smoke"

run_smoke "$(next_step)" "$TOTAL" "llvm_i0_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i1_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i2_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i3_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i4_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i5_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i6_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i7_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i8_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i9_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i10_smoke"
run_smoke "$(next_step)" "$TOTAL" "llvm_i11_smoke"

echo "[test_llvm] I0–I11 OK" >&2
