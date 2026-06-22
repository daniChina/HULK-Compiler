#!/usr/bin/env bash
# Regresion multi-fase (por defecto todos los errores; --first-phase opcional).
set -uo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HULK="$ROOT/hulk"
[[ -x "$ROOT/hulk_c.exe" ]] && HULK="$ROOT/hulk_c.exe"
MANIFEST="$ROOT/tests/multi_phase/manifest.tsv"
FAILURES=0
CHECKS=0

if [[ ! -x "$HULK" ]]; then
  echo "[ERROR] Compilador no encontrado. Ejecuta: make compile" >&2
  exit 1
fi

while IFS=$'\t' read -r id relpath mode expect_exit expect_substrings || [[ -n "$id" ]]; do
  [[ -z "$id" || "$id" == \#* ]] && continue

  file="$ROOT/tests/multi_phase/$relpath"
  if [[ ! -f "$file" ]]; then
    echo "[FAIL] $id — archivo no encontrado: $file"
    FAILURES=$((FAILURES + 1))
    continue
  fi

  args=()
  if [[ "$mode" == "first-phase" ]]; then
    args+=(--first-phase)
  elif [[ "$mode" == "all-errors" ]]; then
    args+=(--all-errors)
  fi
  args+=("$file")

  CHECKS=$((CHECKS + 1))
  stderr="$(mktemp)"
  set +e
  "$HULK" "${args[@]}" 2>"$stderr" >/dev/null
  code=$?
  set -e
  output="$(cat "$stderr")"
  rm -f "$stderr"

  ok=1
  if [[ "$code" -ne "$expect_exit" ]]; then
    echo "[FAIL] $id — exit $code (esperado $expect_exit)"
    ok=0
  fi

  if [[ -n "$expect_substrings" ]]; then
    IFS='|' read -ra parts <<< "$expect_substrings"
    for part in "${parts[@]}"; do
      [[ -z "$part" ]] && continue
      if [[ "$output" != *"$part"* ]]; then
        echo "[FAIL] $id — falta en stderr: $part"
        ok=0
      fi
    done
  fi

  if [[ "$ok" -eq 1 ]]; then
    echo "[OK] $id"
  else
    echo "--- stderr ---"
    echo "$output"
    FAILURES=$((FAILURES + 1))
  fi
done < "$MANIFEST"

echo ""
echo "Checks: $CHECKS  Failures: $FAILURES"
[[ "$FAILURES" -eq 0 ]]
