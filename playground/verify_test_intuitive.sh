#!/usr/bin/env bash
# Verifica mensajes intuitivos T2.4 (playground/test20.hulk)
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HULK="$ROOT/hulk"
[[ -x "$ROOT/hulk_c.exe" ]] && HULK="$ROOT/hulk_c.exe"

if [[ ! -x "$HULK" ]]; then
  echo "[ERROR] Compilador no encontrado. Ejecuta: make build" >&2
  exit 1
fi

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

check() {
  local id="$1"
  local source="$2"
  local expect="$3"
  local exit_code="${4:-2}"

  printf '%s\n' "$source" >"$TMP/case.hulk"
  set +e
  out="$("$HULK" "$TMP/case.hulk" 2>&1)"
  code=$?
  set -e

  if [[ "$code" -eq "$exit_code" ]] && echo "$out" | grep -Fq "$expect"; then
    echo "[OK] $id"
    pass=$((pass + 1))
  else
    echo "[FAIL] $id"
    echo "  codigo: $code (esperado $exit_code)"
    echo "  fragmento: $expect"
    echo "$out" | sed 's/^/    /'
    fail=$((fail + 1))
  fi
}

check_file() {
  local id="$1"
  local file="$2"
  local expect="$3"
  local exit_code="${4:-2}"

  set +e
  out="$("$HULK" "$ROOT/playground/$file" 2>&1)"
  code=$?
  set -e

  if [[ "$code" -eq "$exit_code" ]] && echo "$out" | grep -Fq "$expect"; then
    echo "[OK] $id"
    pass=$((pass + 1))
  else
    echo "[FAIL] $id"
    echo "  archivo: playground/$file"
    echo "  codigo: $code (esperado $exit_code)"
    echo "  fragmento: $expect"
    echo "$out" | sed 's/^/    /'
    fail=$((fail + 1))
  fi
}

echo "== Operador duplicado =="
check T4-dup-op 'print(4++2)' "operador '+' inesperado"
check_file T4-dup-op-file test3.hulk "operador '+' inesperado"

echo ""
echo "== if/while sin parentesis =="
check T4-if 'if true 1 else 2' "se esperaba '(' tras 'if'"
check T4-while 'while true print(1)' "se esperaba '(' tras 'while'"
check_file T4-if-file test6.hulk "se esperaba '(' tras 'if'"

echo ""
echo "== print sin parentesis =="
check T4-print-space 'print 1' "requiere parentesis"

echo ""
echo "== let: bindings =="
check T4-let-bind 'let a=1 b=2 in a' "se esperaba ',' entre bindings o 'in'"
check T4-let-block '{ let a=1; let b=2; a }' "no use ';' entre bindings"

echo ""
echo "== for con rango .. =="
check T4-for-range 'for (i in 1..3) print(i)' "no admite rangos con '..'"

echo ""
echo "== function sin '(' =="
check T4-func-paren 'function f x => 1' "se esperaba '(' con la lista de parametros"

echo ""
echo "== Lexico: cadena sin cerrar (sin mensaje parser confuso) =="
check T4-lex-string 'print(if ("hol) 1 else 2)' "LEXICAL: cadena sin cerrar" 1
check_file T4-lex-file test8.hulk "LEXICAL: cadena sin cerrar" 1

echo ""
echo "== type sin cerrar =="
check T4-type-close 'type T { f() => 1' "falta '}' para cerrar la declaracion de tipo"

echo ""
echo "== function dentro de expresion =="
check T4-func-in-expr 'let f = function() => 1 in f()' "solo puede declararse como sentencia a nivel global"

echo ""
echo "== test13: tras cerrar print falta ';' =="
check_file T4-test13 test13.hulk "se esperaba ';' al final de la sentencia"

echo ""
echo "== Terminales humanizados (while + true) =="
check T4-term 'while true print(1)' "se esperaba '(' tras 'while'"

echo ""
echo "== Resumen =="
echo "OK: $pass  FAIL: $fail"
[[ "$fail" -eq 0 ]]
