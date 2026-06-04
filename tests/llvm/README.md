# Tests de generación LLVM (Fase 4)

**Plan y criterios:** [`docs/tests/fase4_llvm_tests.md`](../../docs/tests/fase4_llvm_tests.md)  
**Paridad y regresión F2/F3:** [`docs/tests/fase4_plan_parity_referencia.md`](../../docs/tests/fase4_plan_parity_referencia.md)  
**Ritual:** cada cambio en `Codegen/` → fixture aquí + `manifest.tsv` + [`docs/codegen/fase4_implementacion.md`](../../docs/codegen/fase4_implementacion.md) + actualizar estados en la matriz de `docs/tests/`.

## Estructura

```text
tests/llvm/
  valid/       # --semantic OK → --llvm genera .ll → (opcional) lli stdout
  invalid/     # no debe producir IR válido o exit ≠ 0 en codegen
  manifest.tsv
```

## Ejecución (objetivo)

```bash
make compile
make test_llvm              # smokes C++ en Codegen/tests/ (pendiente)
make test_llvm_fixtures     # pendiente: hulk --semantic --llvm [+ lli]
```

## Criterio

- **valid:** IR generado sin crash; salida ejecutada alineada a `hulk --interpret` según `docs/tests/fase4_llvm_tests.md`; comparación con el compilador de referencia solo en documentación (`docs/reference/`, `docs/codegen/fase4_decisiones_paridad.md`).
- **invalid:** rechazo en semántica (X1) o fallo documentado en codegen.
- Al añadir un caso **ok**, actualizar la fila de regresión en `docs/tests/fase4_plan_parity_referencia.md` §2 si reutiliza un `.hulk` de `tests/semantic/` o `tests/eval/`.
