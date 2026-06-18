#!/usr/bin/env bash
set -u
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd "$(dirname "$0")/.."

for i in $(seq 1 30); do
  out=$(gdb -batch -ex 'set pagination off' -ex run -ex bt ./llvm_i10_smoke 2>&1) || true
  if echo "$out" | grep -q 'Program received signal'; then
    echo "=== CRASH on run $i ==="
    echo "$out" | tail -40
    exit 0
  fi
done
echo "No crash in 30 runs"
exit 1
