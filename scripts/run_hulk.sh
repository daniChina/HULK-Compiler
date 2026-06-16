#!/usr/bin/env bash
# Ejecuta un archivo .hulk (Linux / macOS / MSYS).
# Uso:
#   ./scripts/run_hulk.sh                         # ejecutar (default)
#   ./scripts/run_hulk.sh --pipeline foo.hulk
#   ./scripts/run_hulk.sh --lexer|--parse|--semantic-only foo.hulk
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HULK="$ROOT/hulk_c.exe"
[[ -x "$ROOT/hulk" ]] && HULK="$ROOT/hulk"
DEFAULT="$ROOT/playground/hello.hulk"

MODE="run"
FILE=""
NO_BUILD=""
NO_AST=""
NO_CST=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --lexer)          MODE="lexer"; shift ;;
    --parse)          MODE="parse"; shift ;;
    --semantic-only)  MODE="semantic"; shift ;;
    --pipeline)       MODE="pipeline"; shift ;;
    --no-build)       NO_BUILD=1; shift ;;
    --no-ast)         NO_AST=1; shift ;;
    --no-cst)         NO_CST=1; shift ;;
    -h|--help)
      cat <<'EOF'
Uso: run_hulk.sh [opciones] [archivo.hulk]

  (default)         Semantica + evaluador
  --lexer           Solo tokens (Fase 0)
  --parse           CST + AST (Fase 1)
  --semantic-only   Solo analisis semantico (Fase 2)
  --pipeline        Las cuatro fases en orden
  --no-ast          En --parse / --pipeline, omitir AST
  --no-cst          En --parse / --pipeline, omitir CST
EOF
      exit 0
      ;;
    *)
      FILE="$1"
      shift
      ;;
  esac
done

[[ -z "$FILE" ]] && FILE="$DEFAULT"

if [[ ! "$FILE" = /* ]]; then
  if [[ -f "$FILE" ]]; then
    FILE="$(cd "$(dirname "$FILE")" && pwd)/$(basename "$FILE")"
  elif [[ -f "$ROOT/$FILE" ]]; then
    FILE="$ROOT/$FILE"
  fi
fi

if [[ ! -f "$FILE" ]]; then
  echo "[ERROR] No se encontro: $FILE" >&2
  exit 1
fi

cd "$ROOT"

if [[ ! -f "$HULK" ]]; then
  if [[ -n "$NO_BUILD" ]]; then
    echo "[ERROR] No existe el compilador. Ejecuta: make compile" >&2
    exit 1
  fi
  echo "Compilando ..."
  make compile || exit 1
  HULK="$ROOT/hulk_c.exe"
  [[ -x "$ROOT/hulk" ]] && HULK="$ROOT/hulk"
fi

run_stage() {
  local title="$1"
  local label="$2"
  shift 2
  local -a args=("$FILE" "$@")

  echo ""
  echo "== $title =="
  echo ""

  OUT="$(mktemp)"
  ERR="$(mktemp)"
  set +e
  "$HULK" "${args[@]}" >"$OUT" 2>"$ERR"
  local code=$?
  set -e

  if [[ -s "$OUT" ]]; then
    echo "-- $label --"
    cat "$OUT"
    echo ""
  fi
  if [[ -s "$ERR" ]]; then
    echo "-- Errores --"
    cat "$ERR"
    echo ""
  fi

  rm -f "$OUT" "$ERR"

  if [[ "$code" -eq 0 ]]; then
    echo "OK (exit 0)"
  else
    echo "FALLO (exit $code)"
  fi
  return "$code"
}

parse_args() {
  PARSE_ARGS=()
  [[ -z "$NO_CST" ]] && PARSE_ARGS+=("--cst")
  [[ -z "$NO_AST" ]] && PARSE_ARGS+=("--ast")
  [[ ${#PARSE_ARGS[@]} -eq 0 ]] && PARSE_ARGS=(--cst --ast)
}

echo ""
echo "== HULK =="
echo "Archivo: $FILE"

case "$MODE" in
  pipeline)
    echo "Modo:    pipeline (lexer -> parser -> semantico -> evaluador)"
    run_stage "1/4 Lexer" "Tokens" --tokens || exit $?
    parse_args
    run_stage "2/4 Parser" "CST / AST" "${PARSE_ARGS[@]}" || exit $?
    run_stage "3/4 Analisis semantico" "Resultado semantico" --semantic || exit $?
    run_stage "4/4 Evaluador" "Salida del programa" --semantic --interpret || exit $?
    ;;
  lexer)
    echo "Modo:    lexer"
    run_stage "Lexer" "Tokens" --tokens || exit $?
    ;;
  parse)
    echo "Modo:    parser"
    parse_args
    run_stage "Parser" "CST / AST" "${PARSE_ARGS[@]}" || exit $?
    ;;
  semantic)
    echo "Modo:    semantico"
    run_stage "Analisis semantico" "Resultado semantico" --semantic || exit $?
    ;;
  *)
    echo "Modo:    ejecutar (semantico + evaluador)"
    run_stage "Evaluador" "Salida del programa" --semantic --interpret || exit $?
    ;;
esac

exit 0
