Sí: hay varias incongruencias entre `playground/Informe_LP.tex` y lo que el compilador hace hoy. Las cuatro extensiones del informe **existen** en lexer, gramática y AST, pero el informe describe una implementación idealizada (desazucarado universal + `for` sobre protocolo `Iterable`) que no coincide del todo con el código.

## Lo que sí cuadra

La **sintaxis** descrita en el informe coincide con la gramática en `Parser/grammar/grammar.ll1`:

| Extensión | Informe | Implementación |
|-----------|---------|----------------|
| `for (x : T in expr) body` | Opcional `: T` | `ForTypeOpt -> COLON IDENTIFIER \| ε` |
| `unless (cond) expr [else alt]` | Sin `elif` | `UnlessExpr -> UNLESS ... ElseOpt` |
| `repeat (n) expr` | Contador numérico | `RepeatExpr -> REPEAT LPAREN Expr RPAREN Expr` |
| `loop expr while (cond)` | Postcondicional | `LoopWhileExpr -> LOOP Expr WHILE LPAREN Expr RPAREN` |

La **semántica observable** de tres de ellas también coincide en el intérprete:

- **`unless`**: ejecuta el cuerpo cuando la condición es falsa; con `else`, ramifica como un `if` invertido.
- **`loop … while`**: el cuerpo corre al menos una vez y repite mientras la condición sea verdadera.
- **`repeat`**: repite `n` veces y devuelve el valor de la última ejecución (p. ej. `repeat (3) 99` → `99`).

La anotación opcional en **`for`** también está en el AST (`ForExpr` con `declared_type` opcional) y el analizador semántico valida conformidad de tipos.

---

## Incongruencias importantes

### 1. El informe afirma desazucarado; el código no lo hace

El `.tex` presenta las cuatro extensiones como **azúcar desazucarado** a `if` / `while` / `let` (secciones 4–7, 9 y 10). En el repositorio **no hay fase de desazucarado**: existen nodos dedicados (`UnlessExpr`, `RepeatExpr`, `LoopWhileExpr`) evaluados directamente en `Evaluator/evaluator.cpp` y analizados en `SemanticCheck/phase2_checker.cpp`.

Eso contradice pasajes como:

> *"el nodo puede ser eliminado por completo durante la fase de desazucarado"*
> *"No requiere cambios en la generación de código intermedio"*

En la práctica, el codegen LLVM tiene **stubs** para esas tres construcciones:

```2597:2599:Codegen/llvm_codegen.cpp
HULK_VISIT_STUB(UnlessExpr)
HULK_VISIT_STUB(RepeatExpr)
HULK_VISIT_STUB(LoopWhileExpr)
```

Un programa con `unless`, `repeat` o `loop … while` compila con `--interpret`, pero **falla en codegen nativo** (`codegen no implementado para …Expr`).

### 2. `for` sobre protocolo `Iterable` vs. solo `range` en ejecución

El informe describe el `for` base y extendido iterando objetos con `next()` y `current()`, desazucarado a:

```text
let _iter = expr in while (_iter.next()) let x [: T] = _iter.current() in body
```

En ejecución real, el intérprete **solo acepta `range`**:

```732:746:Evaluator/evaluator.cpp
void Evaluator::visit(parser::ForExpr* expr) {
  ...
    } else {
        setError("for requiere un iterable (p. ej. range)");
        return;
    }
```

El codegen LLVM hace lo mismo (`for requiere un iterable (p. ej. range)`). Hay tests semánticos con iteradores personalizados (`tests/semantic/valid/73_i7_for_custom_current.hulk`), pero al interpretarlos:

```text
for requiere un iterable (p. ej. range)
```

La inferencia de tipo en semántica sí mira `current()` vía `inferForElementType`, pero eso **no se traduce** a iteración genérica en runtime.

### 3. Estado inicial del compilador: “protocolos”

El informe dice que, antes de las extensiones, ya existían **herencia simple y protocolos** (sección 2). En el código actual, `protocol` no está implementado como característica del lenguaje; hay un tipo `Iterable` en la tabla de símbolos, pero no un sistema de protocolos estructurales completo (como indica `REPORT.md` §6.8).

### 4. `repeat` con `n ≤ 0`: `Null` vs `0`

El informe (§5) dice que el resultado es el último valor evaluado, o **`Null` si `n` es cero o negativo**.

En el intérprete, con `n ≤ 0` el bucle no corre y el resultado es **`0`** (número), no `Null`:

```1021:1030:Evaluator/evaluator.cpp
    const int times = static_cast<int>(current_.asNumber());
    value::Value result(0.0);
    for (int i = 0; i < times && !had_error_; ++i) {
        ...
    }
    current_ = result;
```

Comprobado: `print(repeat (0) 99)` y `print(repeat (-1) 99)` imprimen `0`.

### 5. Casos de prueba descritos vs. cobertura real

El informe (§11) afirma pruebas de `for` tipado sobre **colecciones personalizadas que implementan `Iterable`**. Eso pasa en análisis semántico, pero **no en ejecución** (punto 2).

Los tests dedicados en `tests/extensions/valid/` cubren solo casos mínimos sobre `range` / literales; no validan iteradores OO ni codegen LLVM de las extensiones secundarias.

### 6. Detalle menor: tipado estático de `repeat` y `loop … while`

El informe enfatiza que estas construcciones son expresiones con valor. El analizador semántico les asigna tipo **`Void`**, aunque el intérprete devuelve el último valor del cuerpo. Es una tensión interna del compilador, no del informe en sí, pero conviene saberlo si se usa el chequeo de tipos estrictamente.

---

## Resumen

| Tema | Informe `.tex` | Implementación actual |
|------|----------------|------------------------|
| Estrategia | Desazucarado a `if`/`while`/`let` | Nodos AST + evaluación directa |
| Codegen LLVM | “Sin cambios necesarios” | Stubs para `unless`, `repeat`, `loop … while` |
| `for` | Protocolo `Iterable` (`next`/`current`) | Solo `range` en intérprete y codegen |
| `repeat (n≤0)` | Devuelve `Null` | Devuelve `0` |
| Protocolos en base | Ya existían | No implementados plenamente |
| Tests `Iterable` custom | Descritos como validados end-to-end | Solo semántica; fallan en intérprete |

En conjunto: el informe describe bien la **intención de diseño** y la **sintaxis**, pero sobrestima la **homogeneidad de la implementación** (desazucarado + soporte codegen + `for` genérico). Si quieres alinear documentación y código, los puntos más urgentes son el desajuste desazucarado/realidad, el soporte LLVM pendiente y la semántica de `repeat` con contador no positivo (`Null` vs `0`).