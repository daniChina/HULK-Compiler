#!/usr/bin/env bash
# Ejecuta un comando largo mostrando tiempo transcurrido cada 15 s.
# Uso: run_with_timer.sh [--label NAME] comando [args...]
set -u

LABEL="build"
if [[ "${1:-}" == "--label" ]]; then
    LABEL="$2"
    shift 2
fi

if [[ $# -lt 1 ]]; then
    echo "Uso: run_with_timer.sh [--label NAME] comando [args...]" >&2
    exit 2
fi

start=$SECONDS
echo "[${LABEL}] iniciando (operacion larga; actualizacion cada 15 s)..." >&2

(
    while true; do
        sleep 15
        elapsed=$((SECONDS - start))
        mins=$((elapsed / 60))
        secs=$((elapsed % 60))
        printf '[%s] %d:%02d transcurrido...\n' "$LABEL" "$mins" "$secs" >&2
    done
) &
timer_pid=$!

set +e
"$@"
status=$?
set -e

kill "$timer_pid" 2>/dev/null || true
wait "$timer_pid" 2>/dev/null || true

elapsed=$((SECONDS - start))
mins=$((elapsed / 60))
secs=$((elapsed % 60))
if [[ $status -eq 0 ]]; then
    printf '[%s] terminado OK en %d:%02d\n' "$LABEL" "$mins" "$secs" >&2
else
    printf '[%s] fallo (exit %d) tras %d:%02d\n' "$LABEL" "$status" "$mins" "$secs" >&2
fi
exit $status
