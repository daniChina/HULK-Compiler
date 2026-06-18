#!/usr/bin/env bash
set -u
export PATH=/ucrt64/bin:/usr/bin:$PATH
cd "$(dirname "$0")/.."

for i in $(seq 1 50); do
  out=$(gdb -batch -ex 'set pagination off' -ex run -ex bt --args ./hulk_c.exe tests/llvm/valid/06_as_method.hulk 2>&1) || true
  if echo "$out" | grep -q 'Program received signal'; then
    echo "=== CRASH run $i ==="
    echo "$out" | tail -50
    exit 0
  fi
done
echo "No crash in 50 gdb runs"
exit 1
