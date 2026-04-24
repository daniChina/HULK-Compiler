#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LEXER_BIN="$ROOT_DIR/Lexer/hulk_test"
LEXER_ROOT="$ROOT_DIR/tests/phase_00_lexer_errors"
PHASE="${1:-phase_01_expressions}"

if [[ ! -x "$LEXER_BIN" ]]; then
  echo "No existe ejecutable en $LEXER_BIN"
  echo "Compila primero el lexer. Ejemplo:"
  echo "  cd '$ROOT_DIR/Lexer'"
  echo "  flex++ --c++ -o hulk_lexer.cpp hulk_lexer.l"
  echo "  g++ hulk_lexer.cpp main.cpp -o hulk_test"
  exit 1
fi

run_group() {
  local phase_dir="$1"
  local group="$2"
  local dir="$phase_dir/$group"

  [[ -d "$dir" ]] || return 0

  for f in "$dir"/*.hulk; do
    [[ -e "$f" ]] || continue
    echo "=== ${group^^} ${f#$ROOT_DIR/} ==="
    "$LEXER_BIN" "$f" 2>&1
    echo
  done
}

run_phase() {
  local phase_name="$1"
  local phase_dir="$LEXER_ROOT/$phase_name"

  if [[ ! -d "$phase_dir" ]]; then
    echo "No existe la fase léxica: $phase_name"
    return 1
  fi

  run_group "$phase_dir" valid
  run_group "$phase_dir" invalid
}

if [[ "$PHASE" == "all" ]]; then
  echo "##### phase_00_lexer_errors #####"
  run_group "$LEXER_ROOT" valid
  run_group "$LEXER_ROOT" invalid

  for phase_dir in "$LEXER_ROOT"/phase_*; do
    [[ -d "$phase_dir" ]] || continue
    phase_name="$(basename "$phase_dir")"
    echo "##### $phase_name #####"
    run_phase "$phase_name"
  done
elif [[ "$PHASE" == "phase_00_lexer_errors" ]]; then
  run_group "$LEXER_ROOT" valid
  run_group "$LEXER_ROOT" invalid
else
  run_phase "$PHASE"
fi
