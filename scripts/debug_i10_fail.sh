#!/usr/bin/env bash
set -u
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd "$(dirname "$0")/.."

for i in $(seq 1 50); do
  log="/tmp/i10_run_${i}.log"
  ./llvm_i10_smoke >"$log" 2>&1
  ec=$?
  if [ $ec -ne 0 ]; then
    echo "FAIL run $i exit $ec"
    cat "$log"
    exit 0
  fi
done
echo "50 OK"
