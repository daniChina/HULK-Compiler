#!/usr/bin/env bash
# Recorre tests/semantic/{valid,invalid}/ con hulk.exe --semantic.
# Salida: exit 0 si todo pasa; 1 si algún caso falla.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
HULK="${HULK:-$ROOT/hulk_c.exe}"
VALID_DIR="$ROOT/tests/semantic/valid"
INVALID_DIR="$ROOT/tests/semantic/invalid"
MANIFEST="$ROOT/tests/semantic/manifest.tsv"

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

run_hulk() {
    local file="$1"
    local combined rc
    set +e
    combined="$("$HULK" "$file" --semantic 2>&1)"
    rc=$?
    set -e
    printf '%s' "$combined"
    return "$rc"
}

check_valid() {
    local file="$1"
    local base out rc
    base="$(basename "$file")"
    ran=$((ran + 1))
    set +e
    out="$(run_hulk "$file")"
    rc=$?
    set -e
    if [[ $rc -ne 0 ]]; then
        fail "$base — exit $rc (se esperaba 0)"
        echo "$out" | sed 's/^/  /'
        return
    fi
    if [[ "$out" != *"Semantic OK"* ]]; then
        fail "$base — sin 'Semantic OK'"
        echo "$out" | sed 's/^/  /'
        return
    fi
    pass "valid/$base"
}

# manifest.tsv: invalid/<file>\t<kind>\t<substring>
# kind: semantic | parse | any
lookup_expect() {
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

check_invalid() {
    local file="$1"
    local base out rc kind match expect
    base="$(basename "$file")"
    ran=$((ran + 1))

    if ! expect="$(lookup_expect "$base")"; then
        fail "invalid/$base — sin entrada en manifest.tsv"
        return
    fi
    IFS=$'\t' read -r kind match <<<"$expect"

    set +e
    out="$(run_hulk "$file")"
    rc=$?
    set -e

    if [[ $rc -eq 0 ]]; then
        fail "invalid/$base — exit 0 (se esperaba error)"
        echo "$out" | sed 's/^/  /'
        return
    fi

    case "$kind" in
        semantic)
            if [[ "$out" != *"== Semantic errors =="* && "$out" != *"Error Sem"* ]]; then
                fail "invalid/$base — se esperaba error semántico"
                echo "$out" | sed 's/^/  /'
                return
            fi
            ;;
        parse)
            if [[ "$out" != *"Error de parseo"* && "$out" != *"SYNTACTIC:"* ]]; then
                fail "invalid/$base — se esperaba error de parseo"
                echo "$out" | sed 's/^/  /'
                return
            fi
            ;;
        any) ;;
        *)
            fail "invalid/$base — kind desconocido: $kind"
            return
            ;;
    esac

    if [[ -n "$match" && "$out" != *"$match"* ]]; then
        fail "invalid/$base — salida sin fragmento: $match"
        echo "$out" | sed 's/^/  /'
        return
    fi

    pass "invalid/$base ($kind)"
}

shopt -s nullglob
for f in "$VALID_DIR"/*.hulk; do
    check_valid "$f"
done
for f in "$INVALID_DIR"/*.hulk; do
    check_invalid "$f"
done

echo ""
echo "Casos ejecutados: $ran"
if [[ $failures -gt 0 ]]; then
    echo "Fallos: $failures"
    exit 1
fi
echo "Todos los fixtures semánticos pasaron."
exit 0
