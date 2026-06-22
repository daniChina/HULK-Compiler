#!/usr/bin/env bash
# Recorre tests/eval/{valid,invalid}/ con hulk_c.exe --semantic --interpret.
# Salida: exit 0 si todo pasa; 1 si algún caso falla.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HULK="${HULK:-$ROOT/hulk_c.exe}"
VALID_DIR="$ROOT/tests/eval/valid"
INVALID_DIR="$ROOT/tests/eval/invalid"
MANIFEST="$ROOT/tests/eval/manifest.tsv"

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

normalize_lf() {
    printf '%s' "${1:-}" | tr -d '\r'
}

unescape_expected() {
    local raw="${1:-}"
    raw="${raw//\\n/$'\n'}"
    raw="${raw//\\t/$'\t'}"
    printf '%s' "$raw"
}

run_hulk_eval() {
    local file="$1"
    local stdout_file stderr_file rc
    stdout_file="$(mktemp)"
    stderr_file="$(mktemp)"
    set +e
    "$HULK" "$file" --semantic --interpret >"$stdout_file" 2>"$stderr_file"
    rc=$?
    HULK_STDOUT="$(cat "$stdout_file")"
    HULK_STDERR="$(cat "$stderr_file")"
    rm -f "$stdout_file" "$stderr_file"
    return "$rc"
}

lookup_valid_expect() {
    local base="$1"
    local expect=""
    if [[ ! -f "$MANIFEST" ]]; then
        return 1
    fi
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line//$'\r'/}"
        [[ -z "${line// }" || "$line" =~ ^[[:space:]]*# ]] && continue
        IFS=$'\t' read -r path raw _ <<<"$line"
        path="${path#"${path%%[![:space:]]*}"}"
        path="${path%"${path##*[![:space:]]}"}"
        if [[ "$path" == "valid/$base" ]]; then
            expect="$raw"
            break
        fi
    done < "$MANIFEST"
    if [[ -z "$expect" ]]; then
        return 1
    fi
    printf '%s' "$expect"
    return 0
}

lookup_invalid_expect() {
    local base="$1"
    local kind="" match=""
    if [[ ! -f "$MANIFEST" ]]; then
        return 1
    fi
    while IFS= read -r line || [[ -n "$line" ]]; do
        line="${line//$'\r'/}"
        [[ -z "${line// }" || "$line" =~ ^[[:space:]]*# ]] && continue
        IFS=$'\t' read -r path k m _ <<<"$line"
        path="${path#"${path%%[![:space:]]*}"}"
        path="${path%"${path##*[![:space:]]}"}"
        if [[ "$path" == "invalid/$base" ]]; then
            kind="$k"
            match="$m"
            break
        fi
    done < "$MANIFEST"
    if [[ -z "$kind" ]]; then
        return 1
    fi
    printf '%s\t%s' "$kind" "$match"
    return 0
}

check_valid() {
    local file="$1"
    local base out rc expect_raw expect actual
    base="$(basename "$file")"
    ran=$((ran + 1))

    if ! expect_raw="$(lookup_valid_expect "$base")"; then
        fail "valid/$base — sin entrada en manifest.tsv"
        return
    fi
    expect="$(unescape_expected "$expect_raw")"

    set +e
    run_hulk_eval "$file"
    rc=$?
    set -e
    actual="$(normalize_lf "$HULK_STDOUT")"
    expect="$(normalize_lf "$expect")"

    if [[ $rc -ne 0 ]]; then
        fail "valid/$base — exit $rc (se esperaba 0)"
        [[ -n "$HULK_STDERR" ]] && printf '%s\n' "$HULK_STDERR" | sed 's/^/  /'
        return
    fi
    if [[ "$actual" != "$expect" ]]; then
        fail "valid/$base — stdout distinto al manifest"
        echo "  esperado: $(printf '%q' "$expect")"
        echo "  obtenido: $(printf '%q' "$actual")"
        return
    fi
    pass "valid/$base"
}

check_invalid() {
    local file="$1"
    local base rc kind match expect combined
    base="$(basename "$file")"
    ran=$((ran + 1))

    if ! expect="$(lookup_invalid_expect "$base")"; then
        fail "invalid/$base — sin entrada en manifest.tsv"
        return
    fi
    IFS=$'\t' read -r kind match <<<"$expect"

    set +e
    run_hulk_eval "$file"
    rc=$?
    set -e
    combined="$(normalize_lf "${HULK_STDOUT}${HULK_STDERR}")"

    if [[ $rc -eq 0 ]]; then
        fail "invalid/$base — exit 0 (se esperaba error)"
        echo "$combined" | sed 's/^/  /'
        return
    fi

    case "$kind" in
        runtime)
            if [[ "$combined" != *"Error"* && "$combined" != *"espera"* ]]; then
                fail "invalid/$base — se esperaba error runtime"
                echo "$combined" | sed 's/^/  /'
                return
            fi
            ;;
        semantic)
            if [[ "$combined" != *"== Semantic errors =="* && "$combined" != *"Error Sem"* ]]; then
                fail "invalid/$base — se esperaba error semántico"
                echo "$combined" | sed 's/^/  /'
                return
            fi
            ;;
        parse)
            if [[ "$combined" != *"Error de parseo"* ]]; then
                fail "invalid/$base — se esperaba error de parseo"
                echo "$combined" | sed 's/^/  /'
                return
            fi
            ;;
        any) ;;
        *)
            fail "invalid/$base — kind desconocido: $kind"
            return
            ;;
    esac

    if [[ -n "$match" && "$combined" != *"$match"* ]]; then
        fail "invalid/$base — salida sin fragmento: $match"
        echo "$combined" | sed 's/^/  /'
        return
    fi

    pass "invalid/$base ($kind)"
}

shopt -s nullglob
for f in "$VALID_DIR"/*.hulk; do
    check_valid "$f"
done
if [[ -d "$INVALID_DIR" ]]; then
    for f in "$INVALID_DIR"/*.hulk; do
        check_invalid "$f"
    done
fi

echo ""
echo "Casos ejecutados: $ran"
if [[ $failures -gt 0 ]]; then
    echo "Fallos: $failures"
    exit 1
fi
echo "Todos los fixtures de eval pasaron."
exit 0
