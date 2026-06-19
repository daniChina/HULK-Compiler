#!/usr/bin/env bash
# Oracle Fase 4: hulk (matcom) → ./output ≡ hulk --interpret (stdout).
# Cubre tests/llvm/valid/* del manifest y tests/matcom/oop/*.hulk (I12).
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ -x "$ROOT/hulk" ]]; then
    HULK="${HULK:-$ROOT/hulk}"
else
    HULK="${HULK:-$ROOT/hulk_c.exe}"
fi
MANIFEST="$ROOT/tests/llvm/manifest.tsv"
LLVM_VALID="$ROOT/tests/llvm/valid"
MATCOM_OOP="$ROOT/tests/matcom/oop"

failures=0
ran=0

fail() {
    echo "[FAIL] $*"
    failures=$((failures + 1))
}

pass() {
    echo "[OK] $*"
}

if [[ ! -f "$HULK" ]]; then
    echo "[FAIL] No existe $HULK — ejecutá: make compile" >&2
    exit 1
fi

if ! command -v clang >/dev/null 2>&1; then
    echo "[FAIL] clang no está en PATH (requerido para generar ./output)" >&2
    exit 1
fi

normalize_lf() {
    printf '%s' "${1:-}" | tr -d '\r'
}

output_binary() {
    if [[ -x "$ROOT/output" ]]; then
        printf '%s' "$ROOT/output"
        return 0
    fi
    if [[ -x "$ROOT/output.exe" ]]; then
        printf '%s' "$ROOT/output.exe"
        return 0
    fi
    return 1
}

run_interpret() {
    local file="$1"
    local stdout_file stderr_file rc
    stdout_file="$(mktemp)"
    stderr_file="$(mktemp)"
    set +e
    "$HULK" "$file" --interpret >"$stdout_file" 2>"$stderr_file"
    rc=$?
    set -e
    INTERP_STDOUT="$(cat "$stdout_file")"
    INTERP_STDERR="$(cat "$stderr_file")"
    rm -f "$stdout_file" "$stderr_file"
    return "$rc"
}

run_matcom_compile() {
    local file="$1"
    local stderr_file rc
    rm -f "$ROOT/output" "$ROOT/output.exe" "$ROOT/.hulk_out.ll"
    stderr_file="$(mktemp)"
    set +e
    "$HULK" "$file" >"$stderr_file" 2>&1
    rc=$?
    set -e
    MATCOM_STDERR="$(cat "$stderr_file")"
    rm -f "$stderr_file"
    return "$rc"
}

run_output() {
    local exe stdout_file stderr_file rc
    if ! exe="$(output_binary)"; then
        OUTPUT_STDOUT=""
        OUTPUT_STDERR="no se generó ./output"
        return 1
    fi
    stdout_file="$(mktemp)"
    stderr_file="$(mktemp)"
    set +e
    "$exe" >"$stdout_file" 2>"$stderr_file"
    rc=$?
    set -e
    OUTPUT_STDOUT="$(cat "$stdout_file")"
    OUTPUT_STDERR="$(cat "$stderr_file")"
    rm -f "$stdout_file" "$stderr_file"
    return "$rc"
}

oracle_file() {
    local file="$1"
    local label="$2"
    local out_rc interp_rc actual expect

    ran=$((ran + 1))

    set +e
    run_interpret "$file"
    interp_rc=$?
    set -e
    expect="$(normalize_lf "$INTERP_STDOUT")"

    if [[ $interp_rc -ne 0 ]]; then
        fail "$label — interpret exit $interp_rc"
        [[ -n "$INTERP_STDERR" ]] && printf '%s\n' "$INTERP_STDERR" | sed 's/^/  /'
        return
    fi

    set +e
    run_matcom_compile "$file"
    compile_rc=$?
    set -e
    if [[ $compile_rc -ne 0 ]]; then
        fail "$label — hulk matcom exit $compile_rc (se esperaba 0)"
        [[ -n "$MATCOM_STDERR" ]] && printf '%s\n' "$MATCOM_STDERR" | sed 's/^/  /'
        return
    fi

    set +e
    run_output
    out_rc=$?
    set -e
    actual="$(normalize_lf "$OUTPUT_STDOUT")"

    if [[ $out_rc -ne 0 ]]; then
        fail "$label — ./output exit $out_rc"
        [[ -n "$OUTPUT_STDERR" ]] && printf '%s\n' "$OUTPUT_STDERR" | sed 's/^/  /'
        return
    fi
    if [[ "$actual" != "$expect" ]]; then
        fail "$label — stdout distinto (output vs --interpret)"
        echo "  interpret: $(printf '%q' "$expect")"
        echo "  output:    $(printf '%q' "$actual")"
        return
    fi
    pass "$label"
}

collect_manifest_valid() {
    local paths=()
    if [[ ! -f "$MANIFEST" ]]; then
        return 0
    fi
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line//$'\r'/}"
        [[ -z "${line// }" || "$line" =~ ^[[:space:]]*# ]] && continue
        path="${line%%[[:space:]]*}"
        path="${path#"${path%%[![:space:]]*}"}"
        path="${path%"${path##*[![:space:]]}"}"
        [[ "$path" == valid/* ]] || continue
        paths+=("$ROOT/tests/llvm/$path")
    done < "$MANIFEST"
    if [[ ${#paths[@]} -gt 0 ]]; then
        printf '%s\n' "${paths[@]}"
    fi
}

echo "[test_llvm_fixtures] oracle: ./hulk → ./output ≡ --interpret" >&2

while IFS= read -r file; do
    [[ -f "$file" ]] || continue
    oracle_file "$file" "llvm/$(basename "$file")"
done < <(collect_manifest_valid | sort -u)

shopt -s nullglob
for file in "$MATCOM_OOP"/*.hulk; do
    [[ -f "$file" ]] || continue
    oracle_file "$file" "matcom/oop/$(basename "$file")"
done

echo ""
echo "Casos ejecutados: $ran"
if [[ $failures -gt 0 ]]; then
    echo "Fallos: $failures"
    exit 1
fi
echo "Oracle LLVM + matcom OOP OK."
exit 0
