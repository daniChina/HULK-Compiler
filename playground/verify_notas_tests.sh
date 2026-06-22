#!/usr/bin/env bash
# Verifica fixtures de playground/notas_tests/ (casos de notas_tests.md).
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
NT="$ROOT/playground/notas_tests"
HULK="$ROOT/hulk"
[[ -x "$ROOT/hulk_c.exe" ]] && HULK="$ROOT/hulk_c.exe"
MANIFEST="$NT/manifest.tsv"

if [[ ! -x "$HULK" ]]; then
  echo "[ERROR] Compilador no encontrado. Ejecuta: make build" >&2
  exit 1
fi

PASS=0
FAIL=0

check_row() {
  local id="$1" relpath="$2" kind="$3" expect_exit="$4" expect_msg="$5"
  local file="$NT/$relpath"
  if [[ ! -f "$file" ]]; then
    echo "[FAIL] $id — no existe: $relpath"
    FAIL=$((FAIL + 1))
    return
  fi

  local stderr
  set +e
  stderr="$("$HULK" "$file" 2>&1 >/dev/null)"
  local code=$?
  set -e

  local ok=1
  if [[ "$code" -ne "$expect_exit" ]]; then
    echo "[FAIL] $id — exit $code (esperado $expect_exit)"
    ok=0
  fi

  if [[ "$kind" == "valid" && "$code" -ne 0 ]]; then
    echo "[FAIL] $id — deberia compilar (valid/)"
    ok=0
  fi

  if [[ -n "$expect_msg" && "$expect_msg" != "UNEXPECTED_FAIL" ]]; then
    if [[ "$stderr" != *"$expect_msg"* ]]; then
      echo "[FAIL] $id — falta mensaje: $expect_msg"
      ok=0
    fi
  fi

  if [[ "$ok" -eq 1 ]]; then
    echo "[OK] $id ($kind)"
    PASS=$((PASS + 1))
  else
    echo "--- stderr ($id) ---"
    echo "$stderr" | grep -E '^\([0-9]+,[0-9]+\) (LEXICAL|SYNTACTIC|SEMANTIC):' | head -3
    FAIL=$((FAIL + 1))
  fi
}

while IFS=$'\t' read -r id relpath kind expect_exit expect_msg || [[ -n "$id" ]]; do
  [[ -z "$id" || "$id" == \#* ]] && continue
  check_row "$id" "$relpath" "$kind" "$expect_exit" "$expect_msg"
done < "$MANIFEST"

echo ""
echo "OK: $PASS  FAIL: $FAIL  (total $((PASS + FAIL)))"
[[ "$FAIL" -eq 0 ]]
