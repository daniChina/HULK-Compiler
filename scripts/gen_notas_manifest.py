#!/usr/bin/env python3
"""Regenera playground/notas_tests/manifest.tsv ejecutando ./hulk_c.exe sobre cada fixture."""
from __future__ import annotations

import glob
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HULK = os.path.join(ROOT, "hulk_c.exe")
if not os.path.isfile(HULK):
    HULK = os.path.join(ROOT, "hulk")
NT = os.path.join(ROOT, "playground", "notas_tests")
MANIFEST = os.path.join(NT, "manifest.tsv")


def first_diagnostic(out: str) -> str:
    for line in out.splitlines():
        for phase in ("SEMANTIC:", "SYNTACTIC:", "LEXICAL:"):
            if line.startswith("(") and phase in line:
                return line.split(phase, 1)[1].strip()[:80]
    return ""


def main() -> int:
    if not os.path.isfile(HULK):
        print("Compilador no encontrado. Ejecuta: make build", file=sys.stderr)
        return 1

    rows = ["# id\tpath\tkind\texit\texpect"]
    for kind in ("invalid", "valid"):
        for path in sorted(glob.glob(os.path.join(NT, kind, "*.hulk"))):
            rel = os.path.relpath(path, NT)
            case_id = os.path.splitext(os.path.basename(path))[0]
            proc = subprocess.run([HULK, path], capture_output=True, text=True)
            out = proc.stdout + proc.stderr
            if kind == "valid":
                exp, expect = 0, ""
                if proc.returncode != 0:
                    expect = "UNEXPECTED_FAIL"
            else:
                exp = proc.returncode
                expect = first_diagnostic(out)
            rows.append(f"{case_id}\t{rel}\t{kind}\t{exp}\t{expect}")

    with open(MANIFEST, "w", encoding="utf-8") as f:
        f.write("\n".join(rows) + "\n")
    print(f"Wrote {len(rows) - 1} entries to {MANIFEST}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
