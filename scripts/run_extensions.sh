#!/usr/bin/env bash
# E2E para tests/extensions/valid/: --interpret y oracle nativo (./output ≡ --interpret).
set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if [[ -x "$ROOT/hulk" ]]; then
    HULK="${HULK:-$ROOT/hulk}"
else
    HULK="${HULK:-$ROOT/hulk_c.exe}"
fi
MANIFEST="$ROOT/tests/extensions/manifest.tsv"
EXT_VALID="$ROOT/tests/extensions/valid"

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
    echo "[FAIL] No existe $HULK — ejecutá: make compile && make build" >&2
    exit 1
fi

if ! command -v clang >/dev/null 2>&1; then
    echo "[WARN] clang no está en PATH — se omitirá oracle nativo" >&2
    HAVE_CLANG=0
else
    HAVE_CLANG=1
fi

normalize_lf() {
    printf '%s' "${1:-}" | tr -d '\r'
}

unescape_expected() {
    local raw="${1:-}"
    raw="${raw//\\n/$'\n'}"
    raw="${raw//\\t/$'\t'}"
    printf '%s' "$raw"
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
    "$HULK" "$file" --interpret >"$stdout_file" 2>"$stderr_file"
    rc=$?
    INTERP_STDOUT="$(cat "$stdout_file")"
    INTERP_STDERR="$(cat "$stderr_file")"
    rm -f "$stdout_file" "$stderr_file"
    return "$rc"
}

run_compile() {
    local file="$1"
    local stderr_file rc
    rm -f "$ROOT/output" "$ROOT/output.exe" "$ROOT/.hulk_out.ll"
    stderr_file="$(mktemp)"
    "$HULK" "$file" >"$stderr_file" 2>&1
    rc=$?
    COMPILE_STDERR="$(cat "$stderr_file")"
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
    "$exe" >"$stdout_file" 2>"$stderr_file"
    rc=$?
    OUTPUT_STDOUT="$(cat "$stdout_file")"
    OUTPUT_STDERR="$(cat "$stderr_file")"
    rm -f "$stdout_file" "$stderr_file"
    return "$rc"
}

lookup_manifest() {
    local base="$1"
    local expect="" mode=""
    if [[ ! -f "$MANIFEST" ]]; then
        return 1
    fi
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line//$'\r'/}"
        [[ -z "${line// }" || "$line" =~ ^[[:space:]]*# ]] && continue
        IFS=$'\t' read -r path raw mode <<<"$line"
        path="${path#"${path%%[![:space:]]*}"}"
        path="${path%"${path##*[![:space:]]}"}"
        if [[ "$path" == "valid/$base" ]]; then
            expect="$raw"
            mode="${mode:-both}"
            break
        fi
    done < "$MANIFEST"
    if [[ -z "$mode" && -z "$expect" ]]; then
        return 1
    fi
    printf '%s\t%s' "$expect" "$mode"
    return 0
}

check_case() {
    local file="$1"
    local base label expect_raw mode expect actual
    base="$(basename "$file")"
    label="extensions/$base"
    ran=$((ran + 1))

    if ! manifest_line="$(lookup_manifest "$base")"; then
        fail "$label — sin entrada en manifest.tsv"
        return
    fi
    IFS=$'\t' read -r expect_raw mode <<<"$manifest_line"
    mode="${mode:-both}"

    run_interpret "$file"
    interp_rc=$?
    if [[ $interp_rc -ne 0 ]]; then
        fail "$label — interpret exit $interp_rc"
        [[ -n "$INTERP_STDERR" ]] && printf '%s\n' "$INTERP_STDERR" | sed 's/^/  /'
        return
    fi

    if [[ "$mode" == "compile" ]]; then
        if [[ $HAVE_CLANG -eq 0 ]]; then
            pass "$label (compile-only, interpret OK; sin clang)"
            return
        fi
        run_compile "$file"
        compile_rc=$?
        if [[ $compile_rc -ne 0 ]]; then
            fail "$label — hulk nativo exit $compile_rc"
            [[ -n "$COMPILE_STDERR" ]] && printf '%s\n' "$COMPILE_STDERR" | sed 's/^/  /'
            return
        fi
        pass "$label (compile-only)"
        return
    fi

    expect="$(normalize_lf "$(unescape_expected "$expect_raw")")"
    if [[ "$expect_raw" == "-" ]]; then
        expect=""
    fi
    actual="$(normalize_lf "$INTERP_STDOUT")"
    if [[ "$actual" != "$expect" ]]; then
        fail "$label — stdout interpret distinto al manifest"
        echo "  esperado: $(printf '%q' "$expect")"
        echo "  obtenido: $(printf '%q' "$actual")"
        return
    fi

    if [[ $HAVE_CLANG -eq 0 ]]; then
        pass "$label (interpret OK; sin clang)"
        return
    fi

    run_compile "$file"
    compile_rc=$?
    if [[ $compile_rc -ne 0 ]]; then
        fail "$label — hulk nativo exit $compile_rc"
        [[ -n "$COMPILE_STDERR" ]] && printf '%s\n' "$COMPILE_STDERR" | sed 's/^/  /'
        return
    fi

    run_output
    out_rc=$?
    actual="$(normalize_lf "$OUTPUT_STDOUT")"
    if [[ $out_rc -ne 0 ]]; then
        fail "$label — ./output exit $out_rc"
        [[ -n "$OUTPUT_STDERR" ]] && printf '%s\n' "$OUTPUT_STDERR" | sed 's/^/  /'
        return
    fi
    if [[ "$actual" != "$expect" ]]; then
        fail "$label — stdout distinto (output vs manifest)"
        echo "  esperado: $(printf '%q' "$expect")"
        echo "  output:   $(printf '%q' "$actual")"
        return
    fi
    pass "$label"
}

echo "[run_extensions] E2E: tests/extensions/valid/ (interpret + oracle nativo)" >&2

shopt -s nullglob
for file in "$EXT_VALID"/*.hulk; do
    [[ -f "$file" ]] || continue
    check_case "$file"
done

echo ""
echo "Casos ejecutados: $ran"
if [[ $failures -gt 0 ]]; then
    echo "Fallos: $failures"
    exit 1
fi
echo "Extensiones E2E OK."
exit 0
