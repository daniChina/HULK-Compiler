#!/usr/bin/env bash
# Verifica mensajes sintacticos amigables catalogados en playground/test.hulk
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
  local code=$?
  set -e

  if [[ "$code" -eq "$exit_code" ]] && echo "$out" | grep -Fq "$expect"; then
    echo "[OK] $id"
    pass=$((pass + 1))
  else
    echo "[FAIL] $id"
    echo "  codigo: $code (esperado $exit_code)"
    echo "  fragmento esperado: $expect"
    echo "  salida:"
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
  local code=$?
  set -e

  if [[ "$code" -eq "$exit_code" ]] && echo "$out" | grep -Fq "$expect"; then
    echo "[OK] $id"
    pass=$((pass + 1))
  else
    echo "[FAIL] $id"
    echo "  archivo: playground/$file"
    echo "  codigo: $code (esperado $exit_code)"
    echo "  fragmento esperado: $expect"
    echo "  salida:"
    echo "$out" | sed 's/^/    /'
    fail=$((fail + 1))
  fi
}

echo "== Grupo 1: se esperaba ';' al final de la sentencia (EOF) =="
check G1-print 'print(1)' "se esperaba ';' al final de la sentencia"
check G1-pi 'pi' "se esperaba ';' al final de la sentencia"
check G1-add '1+2' "se esperaba ';' al final de la sentencia"
check G1-new 'new Point(1,2)' "se esperaba ';' al final de la sentencia"
check G1-let 'let x = 1 in x' "se esperaba ';' al final de la sentencia"
check G1-if 'if (true) 1 else 2' "se esperaba ';' al final de la sentencia"
check G1-while 'while (true) print(1)' "se esperaba ';' al final de la sentencia"
check G1-unless 'unless (false) 1' "se esperaba ';' al final de la sentencia"
check G1-repeat 'repeat (2) print(1)' "se esperaba ';' al final de la sentencia"
check G1-with 'with (1 as x) x' "se esperaba ';' al final de la sentencia"
check G1-case 'case 1 of a: Number => 1' "se esperaba ';' al final de la sentencia"
check G1-assign '1 := 2' "se esperaba ';' al final de la sentencia"
check G1-two 'print(1); print(2)' "se esperaba ';' al final de la sentencia"
check G1-func 'function f() => 1' "se esperaba ';' al final de la sentencia"
check G1-loop 'loop print(1) while (true)' "se esperaba ';' al final de la sentencia"
check G1-func-loop 'function f() => loop print(1) while (true)' "se esperaba ';' al final de la sentencia"
check_file G1-test2 test2.hulk "se esperaba ';' al final de la sentencia"

echo ""
echo "== Grupo 2: falta ')' (EOF con parentesis abierto) =="
check G2-paren 'print(1' "falta ')' para cerrar la expresion o la llamada a funcion"
check G2-print-let 'print(let a=1 in a' "falta ')' para cerrar la expresion o la llamada a funcion"
check_file G2-test18 test18.hulk "falta ')' para cerrar la expresion o la llamada a funcion"

echo ""
echo "== Grupo 3: se esperaba ';' antes de '}' o entre ramas case =="
check G3-block '{ print(1) }' "se esperaba ';' antes de '}'"
check G3-block2 '{ print(1); print(2) }' "se esperaba ';' antes de '}'"
check G3-let-block 'let x = 1 in { print(1) }' "se esperaba ';' antes de '}'"
check G3-method 'type T { m() => 1 }' "se esperaba ';' antes de '}'"
check G3-attr 'type T { x: Number = 1 }' "se esperaba ';' antes de '}'"
check G3-case-block 'case 1 of { a: Number => 1; b: Number => 2 }' "se esperaba ';' antes de '}'"
check G3-loop-block '{ loop print(1) while (true) }' "se esperaba ';' antes de '}'"
check G3-case-branch 'case 1 of { a: Number => 1 b: Number => 2 }' "se esperaba ';' entre ramas del case"
check_file G3-test19 test19.hulk "se esperaba ';' antes de '}'"

echo ""
echo "== T2.2: ArgListTail + SEMICOLON (test12) =="
check_file T2.2-test12 test12.hulk "';' inesperado dentro de la llamada a funcion"

echo ""
echo "== T2.3: PostfixTail + EQUAL (test11) =="
check_file T2.3-test11 test11.hulk "no se puede usar '=' con 'c' aqui"

echo ""
echo "== Resumen =="
echo "OK: $pass  FAIL: $fail"
[[ "$fail" -eq 0 ]]
