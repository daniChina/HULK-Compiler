# Tests del intérprete (Fase 3)

**Plan y criterios:** [`docs/tests/fase3_eval_tests.md`](../../docs/tests/fase3_eval_tests.md)  
**Ritual:** cada cambio en el evaluador → fixture aquí + `manifest.tsv` + [`docs/runtime/fase3_implementacion.md`](../../docs/runtime/fase3_implementacion.md).

## Estructura

```text
tests/eval/
  valid/     # exit 0; stdout según manifest
  invalid/   # exit ≠ 0 o error runtime (pocos casos)
  manifest.tsv
```

## Ejecución (objetivo)

```bash
make compile
make test_eval              # smokes C++
make test_eval_fixtures     # pendiente: hulk --semantic --interpret
```

## Criterio

Salida según el plan de pruebas en `docs/tests/` (comparación con el compilador de referencia documentada en [`docs/reference/compilador_referencia.md`](../../docs/reference/compilador_referencia.md)). Divergencias intencionales: [`docs/runtime/fase3_decisiones_paridad.md`](../../docs/runtime/fase3_decisiones_paridad.md).
