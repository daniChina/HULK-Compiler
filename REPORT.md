# Compilador HULK — Informe de arquitectura

**Daniela De La Caridad Guerrero Álvarez · Rubén Martínez Rojas · Sammy Raul Sosa Justiz**  
Facultad de Matemática y Computación — Universidad de La Habana · 2025–2026

---

## 1. Introducción

HULK (Havana University Language for Kompilers) es un lenguaje de programación académico diseñado para el curso de Compilación de la Universidad de La Habana. El objetivo de este proyecto es construir un compilador funcional que recorra las fases clásicas de la teoría de compiladores: análisis léxico, análisis sintáctico, análisis semántico y generación de código.

HULK es un lenguaje orientado a expresiones. Los programas consisten en cero o más declaraciones de nivel superior (funciones globales y definiciones de tipos) seguidas de una o más expresiones ejecutables. El lenguaje soporta operadores aritméticos, lógicos y de comparación; estructuras de control como `if`/`elif`/`else`, `while`, `for`, `unless`, `repeat`, `loop … while`, `with` y `case`; declaración de funciones con tipo de retorno opcional; tipos orientados a objetos con herencia simple; expresiones de ligadura con `let … in`; bloques de expresiones; pruebas de tipo con `is` y conversiones con `as`; y asignación destructiva con `:=`. Las extensiones `unless`, `repeat`, `loop … while` y `for` con tipo opcional se documentan en la sección 6. Funciones predefinidas como `print`, `sin`, `cos` y `range` forman parte de la biblioteca estándar.

Este informe describe la arquitectura del compilador, las decisiones de diseño en cada etapa, las funcionalidades implementadas, las limitaciones conocidas y la metodología de pruebas empleada.

---

## 2. Arquitectura general

El compilador sigue un pipeline convencional de múltiples fases. El código fuente entra por un lexer basado en Flex, es analizado por un parser LL(1) dirigido por tabla que produce un árbol de sintaxis concreto (CST) y un árbol de sintaxis abstracto (AST), pasa por un analizador semántico de dos etapas y, finalmente, genera código máquina nativo mediante LLVM o se ejecuta directamente por un intérprete de recorrido del árbol.

```
archivo.hulk
   → Lexer (Flex++)
   → Parser LL(1)  — grammar.ll1 + tabla de análisis
   → CST → AST     — transformación cst_to_ast
   → Análisis semántico (tabla de símbolos + verificador de tipos)
   → Codegen LLVM  → .hulk_out.ll → clang + runtime.c → ./output
   → [opcional]    Intérprete de recorrido del árbol (--interpret)
```

### 2.1 Organización del repositorio

| Directorio | Responsabilidad |
|------------|-----------------|
| `Lexer/` | Especificación Flex y escáner generado |
| `Parser/` | Motor LL(1), archivo de gramática, nodos CST/AST |
| `SemanticCheck/` | Verificador de tipos y gestor de errores |
| `SymbolTable/` | Gestión de declaraciones y ámbitos |
| `Types/` | Representación de tipos y subtipado |
| `Value/` | Modelo de valores en tiempo de ejecución del intérprete |
| `Evaluator/` | Intérprete de recorrido del árbol |
| `Codegen/` | Generador de IR LLVM y runtime en C |
| `Compiler/` | Punto de entrada, orquestación del pipeline, diagnósticos |
| `tests/` | Suites de pruebas semánticas, de evaluación y de codegen |

### 2.2 Interfaz de línea de comandos

El binario `./hulk` soporta dos modos de operación. Invocado como `./hulk archivo.hulk`, ejecuta el pipeline de producción: analiza el programa y, si no hay errores, emite un ejecutable nativo `./output` en el directorio actual. Flags de desarrollo (`--interpret`, `--semantic`, `--tokens`, `--ast`) producen salida diagnóstica adicional sin alterar el comportamiento por defecto.

---

## 3. Análisis léxico

### 3.1 Función del lexer

El lexer lee el archivo fuente y lo transforma en una secuencia de **tokens** — pares formados por un tipo y, cuando aplica, un valor semántico. Además de tokenizar, mantiene la posición actual (línea y columna), descarta espacios en blanco y comentarios, y reporta errores léxicos ante caracteres no reconocidos o construcciones sin terminar.

### 3.2 Implementación con Flex

El lexer se implementó con **Flex** (`Lexer/hulk_lexer.l`). Flex genera un escáner en C++ a partir de expresiones regulares y aplica la política de **coincidencia más larga** (*longest match*), lo que resuelve ambigüedades como distinguir `:=` de `:` seguido de `=`. La opción `nodefault` desactiva la acción silenciosa por defecto; los caracteres no reconocidos se manejan con una regla de respaldo que reporta error léxico.

Las reglas cubren espacios en blanco, comentarios de línea y bloque (con detección de bloques sin cerrar), literales de cadena con secuencias de escape, literales numéricos (flotantes antes que enteros), booleanos, palabras clave (declaradas antes que identificadores), operadores multi-carácter y fin de archivo.

Cada token lleva una estructura `SemanticValue` con campos para cadenas, números y booleanos. Los contadores `line_` y `column_` se copian en la estructura `Token` del parser para que todos los mensajes de error incluyan ubicaciones precisas. Los caracteres no reconocidos producen diagnósticos de la forma `(línea,col) LEXICAL: unexpected character 'X'` con código de salida 1.

### 3.3 Adaptador de tokens

La salida del lexer se convierte mediante un adaptador (`Parser/core/token_adapter.*`) en una estructura uniforme `Token` con campos `type`, `lexeme`, `line` y `col`. La lista completa se envuelve en un `TokenStream` que provee `current()`, `advance()`, `peek(n)` y `consume(type, msg)`. Esta capa de abstracción desacopla el lexer Flex del parser LL(1) y garantiza que toda la información de posición fluya de forma consistente hasta los diagnósticos finales, independientemente de en qué fase se detecte un error.

---

## 4. Análisis sintáctico

### 4.1 Función del parser

El parser verifica que la secuencia de tokens cumple la gramática de HULK y construye dos representaciones en árbol: el **CST**, que refleja las derivaciones gramaticales incluyendo no terminales auxiliares, y el **AST**, que elimina el andamiaje gramatical y conserva solo la estructura semánticamente relevante.

### 4.2 Parser LL(1)

Implementamos un **parser LL(1) dirigido por tabla**: lee la entrada de izquierda a derecha, produce una derivación por la izquierda y usa un token de anticipación para seleccionar producciones. Esta elección conecta directamente con los conceptos del curso (conjuntos FIRST y FOLLOW, tablas predictivas) y separa la definición de la gramática (`Parser/grammar/grammar.ll1`) del algoritmo de parseo.

La gramática formal reside en `grammar.ll1` y el parser construye la tabla LL(1) dinámicamente mediante un generador propio (`Parser/generator/`) que calcula conjuntos FIRST/FOLLOW y detecta conflictos. Nuestra gramática actual tiene cero conflictos, verificado por prueba automatizada.

**Precedencia de operadores.** Se eliminó la recursión izquierda mediante colas estándar para suma/resta y multiplicación/división (asociatividad izquierda). La exponenciación usa una cola recursiva a la derecha para que `2^3^2` se parsee como `2^(3^2)`.

**Desambiguación del cuerpo de clase.** Atributos y métodos comienzan con un identificador; con un solo token de anticipación se factoriza por la izquierda: tras consumir el identificador, `COLON` indica atributo y `LPAREN` indica método.

**Restricción de bloques.** `FIRST(Expr)` no incluye `LBRACE`, por lo que los bloques `{…}` aparecen solo en posiciones explícitamente permitidas.

Los errores sintácticos se representan como excepciones `ParseError` y se formatean como `(línea,col) SYNTACTIC: …` con código de salida 2. El parser incluye recuperación a nivel de sentencia para construir un AST parcial útil en diagnósticos.

### 4.3 Conversión CST a AST

La conversión (`Parser/ast/cst_to_ast.cpp`) recorre el CST recursivamente. Bajo un nodo raíz `Program` emergen sentencias (`Stmt`) — funciones, tipos, expresiones de nivel superior — y expresiones (`Expr`). Transformaciones notables: cadenas de cola de operadores se aplanan en nodos `BinaryExpr`; accesos y llamadas postfix se convierten en `GetAttrExpr` y `CallExpr` anidados; ligaduras múltiples en `let` se anidan; ramas `elif` se normalizan a `IfExpr` anidados; las llamadas a método y asignación a atributo se unifican mediante `GetAttrExpr` combinado con `CallExpr` o `AssignExpr`, simplificando las fases posteriores.

Los tipos de nodo del AST cubren literales, identificadores, operadores, `let`, bloques, todas las estructuras de control del lenguaje, declaraciones de funciones y tipos, `is`/`as`, `new` y asignación destructiva.

### 4.4 El parser predictivo

El parser LL(1) (`Parser/syntax/ll1_parser.*`) mantiene una pila inicializada con el símbolo inicial `Program`. En cada paso empareja un terminal con el token actual o expande un no terminal usando la entrada de tabla correspondiente al próximo token, sin haberlo consumido aún. El CST se construye simultáneamente: los nodos internos corresponden a no terminales y las hojas a terminales consumidos. Cuando el token actual no coincide con ninguna producción aplicable, se lanza un `ParseError` con mensaje contextual — por ejemplo, indicando que falta un punto y coma al final de una sentencia en lugar de un genérico «token inesperado».

---

## 5. Análisis semántico

### 5.1 Estructura de dos pasadas

El análisis semántico se divide en recolección de declaraciones y verificación de tipos. La primera pasada (`decl_collector.cpp`) registra firmas de funciones (con sobrecarga por aridad), nombres de clases, herencia, atributos y métodos. Las funciones predefinidas y constantes globales (`PI`, `E`) se inyectan antes del código del usuario. La segunda pasada (`phase2_checker.cpp`) valida el programa completo.

### 5.2 Reglas núcleo

**R1 — Una definición por ámbito.** Un nombre de variable solo puede declararse una vez dentro del mismo ámbito `let`. La asignación destructiva `:=` reasigna un enlace existente. Las variables globales predefinidas no pueden redefinirse.

**R2 — Definida antes de uso.** Los enlaces en `let` se procesan de izquierda a derecha. La autorreferencia en un inicializador se rechaza. Las funciones son globales y no capturan variables de ámbitos `let` exteriores.

**R3 — Parámetros únicos y sobrecarga.** Los nombres de parámetros deben ser únicos dentro de una firma. Varias funciones globales pueden compartir nombre si difieren en aridad.

**R4 — Orden textual de declaración.** Una llamada a una función aún no declarada produce `UNDEFINED_FUNCTION`. Las funciones predefinidas están precargadas.

### 5.3 Semántica orientada a objetos

Para cada declaración `type`, el analizador valida herencia, detecta ciclos, registra atributos y analiza métodos en un ámbito donde `self` tiene el tipo de la clase actual. Las comprobaciones de sobrescritura comparan tipos de retorno y parámetros con los métodos del padre a lo largo de toda la cadena de herencia. `new` se valida contra la lista de parámetros del constructor; las expresiones de inicialización de atributos se verifican en el contexto de la clase. `base()` está permitido solo dentro de métodos de clases que declaran un padre, y su invocación se restringe al cuerpo de métodos donde tiene sentido delegar en la implementación heredada.

La tabla de símbolos (`SymbolTable/`) gestiona ámbitos anidados para `let`, cuerpos de función, métodos y bloques. Cada ámbito mantiene un mapa de enlaces locales y referencias a su ámbito padre. Las declaraciones de tipo viven en un registro global accesible durante todo el análisis, lo que permite resolver referencias cruzadas entre clases declaradas en cualquier orden dentro de la unidad de compilación.

### 5.4 Inferencia de tipos y sistema de tipos

HULK permite omitir anotaciones de tipo en variables locales, retornos de funciones y atributos. El analizador ejecuta hasta diez pasadas de punto fijo sobre cuerpos de funciones y métodos hasta estabilizar tipos. `Types/type_info.cpp` modela `Number`, `String`, `Boolean`, `Null`, tipos objeto y tipos inferidos; el subtipado se verifica mediante `conformsTo`. El operador `is` produce `Boolean`; `as` produce el tipo objetivo cuando el cast es estáticamente posible.

### 5.5 Estructuras de control del lenguaje

Todas las construcciones de control son **expresiones** con valor de retorno. `if`/`elif`/`else` y `while` siguen la semántica estándar. `with (expr as alias) cuerpo` introduce el alias solo si el valor no es `Null`. `case expr of { rama: Tipo => cuerpo; … }` selecciona la rama cuyo tipo es el ancestro más cercano entre las que coinciden con el tipo dinámico de la expresión. El analizador verifica tipos de condiciones, legibilidad de alias y alcanzabilidad estática de ramas en `case`. Cuatro extensiones adicionales de control de flujo e iteración se describen en la sección 6.

### 5.6 Funciones predefinidas

El analizador precarga un conjunto de funciones y constantes globales antes de procesar el código del usuario. Entre las funciones builtin están `print` (acepta cualquier tipo imprimible), funciones matemáticas (`sin`, `cos`, `log`, `sqrt`, `exp`), y `range(inicio, fin)` que produce un iterable numérico en el intervalo `[inicio, fin)`. Las constantes `@PI` y `@E` tienen tipo `Number`. Estas declaraciones no pueden redefinirse ni sobrecargarse; cualquier intento de redeclaración produce error semántico. Las llamadas a builtins se resuelven en tiempo de compilación cuando los argumentos son conocidos estáticamente, y en tiempo de ejecución en los casos generales.

### 5.7 Reporte de errores

El compilador reporta diagnósticos con formato `(línea,col) FASE: mensaje` en stderr, donde `FASE` es `LEXICAL`, `SYNTACTIC` o `SEMANTIC`. Por defecto acumula errores de todas las fases alcanzables, los ordena por posición y deduplica; el código de salida refleja la fase más fundamental (léxico > sintáctico > semántico). Con `--first-phase` solo se muestran errores de la primera fase que falla. No se genera `./output` si la compilación falla.

---

## 6. Extensiones de control de flujo e iteración

Además del núcleo de HULK (`if`, `while`, `for … in range`), el compilador incorpora **cuatro extensiones** que amplían las formas de expresar condicionales y bucles. Todas son expresiones con valor de retorno, se integran en la gramática LL(1) (`Parser/grammar/grammar.ll1`), producen nodos dedicados en el AST, pasan por el analizador semántico y están implementadas de extremo a extremo en intérprete y codegen LLVM. Ejemplos ejecutables se encuentran en `tests/extensions/valid/`; casos adicionales en `tests/eval/valid/` y `tests/llvm/valid/`.

### 6.1 `unless` — condicional invertida

**Sintaxis:** `unless (condición) expresión` o `unless (condición) expresión else alternativa`.

**Qué aporta.** Equivalente a `if (!(condición)) …`: el cuerpo principal se ejecuta cuando la condición es **falsa**. La condición debe ser `Boolean`.

**Ejemplo:**

```hulk
unless (false) 42 else 0;           // produce 42
print(unless (false) 1 else 2);     // imprime 1
```

**Archivos de referencia:** `tests/extensions/valid/unless.hulk`, `unless_then.hulk`, `unless_else.hulk`; `tests/eval/valid/43_v9_unless.hulk`; `tests/llvm/valid/83_l8_unless.hulk`, `90_l9_unless_branches.hulk`.

### 6.2 `repeat` — repetición contada

**Sintaxis:** `repeat (n) cuerpo`.

**Qué aporta.** Evalúa `cuerpo` exactamente `n` veces, donde `n` es de tipo `Number`. El valor de la expresión es el de la **última** ejecución; si `n ≤ 0`, el resultado es `Null` sin ejecutar el cuerpo.

**Ejemplo:**

```hulk
print(let n = 0 in { repeat (3) n := n + 1; n; });   // imprime 3
```

**Archivos de referencia:** `tests/extensions/valid/repeat.hulk`, `repeat_count.hulk`, `repeat_zero.hulk`; `tests/eval/valid/44_v9_repeat.hulk`; `tests/llvm/valid/84_l8_repeat.hulk`, `91_l9_repeat_edges.hulk`.

### 6.3 `loop … while` — bucle con evaluación posterior

**Sintaxis:** `loop cuerpo while (condición)`.

**Qué aporta.** Similar al `do … while` de C: el cuerpo se ejecuta **al menos una vez** antes de comprobar la condición; mientras sea verdadera, se repite. La condición debe ser `Boolean`.

**Ejemplo:**

```hulk
print(loop 7 while (false));   // imprime 7 (una sola ejecución)
```

**Archivos de referencia:** `tests/extensions/valid/loop_while.hulk`, `loop_while_once.hulk`, `loop_while_multi.hulk`; `tests/eval/valid/45_v9_loop_while.hulk`; `tests/llvm/valid/85_l8_loop_while.hulk`, `92_l9_loop_while_multi.hulk`.

### 6.4 `for` con tipo opcional e iterables definidos por el usuario

**Sintaxis ampliada:** `for (x in iterable) cuerpo` o `for (x: Tipo in iterable) cuerpo`.

**Qué aporta.** La forma base `for (x in range(0, n))` ya pertenece a HULK; la extensión añade la cláusula opcional `: Tipo` tras el identificador del iterador y permite iterar sobre objetos que implementan `next(): Boolean` y `current(): T`, no solo sobre `range`.

**Ejemplo:**

```hulk
for (x: Number in range(0, 3)) print(x);   // imprime 0, 1, 2
```

Para iterables personalizados, ver `tests/extensions/valid/for_custom_iter.hulk` y `for_string_iter.hulk`.

**Archivos de referencia:** `tests/extensions/valid/for_range.hulk`, `for_typed.hulk`, `for_custom_iter.hulk`, `for_countdown_iter.hulk`; `tests/eval/valid/98_v16_for_custom_iter.hulk`, `99_v17_for_countdown.hulk`; `tests/llvm/valid/81_l8_range_for.hulk`, `82_l8_for_custom_iter.hulk`, `89_l9_for_countdown.hulk`, `98_l9_for_string_iter.hulk`.

Las tres primeras extensiones (`unless`, `repeat`, `loop … while`) pueden entenderse como azúcar sintáctica sobre `if` y `while`; la cuarta refuerza el sistema de tipos estático y el protocolo de iteración orientado a objetos sin exigir anotaciones cuando el iterable es conocido.

---

## 7. Intérprete

Junto con la generación de código, el compilador incluye un intérprete de recorrido del árbol (`Evaluator/evaluator.cpp`) que ejecuta el AST directamente. Sirve para desarrollo y validación: su salida se compara con el binario nativo `./output` para verificar equivalencia de comportamiento.

El módulo `Value/` define un tipo `Value` con etiquetas para `Number`, `String`, `Boolean`, `Null`, `Range` e instancias objeto. Los enlaces se gestionan mediante una cadena de marcos de entorno (`env_frame.hpp`); la asignación destructiva recorre marcos ancestros. Las funciones de usuario se almacenan en un mapa indexado por nombre y aridad.

El evaluador procesa un `Program` en dos pasadas: primero registra declaraciones de tipo, luego ejecuta funciones de nivel superior y la expresión final. Todas las estructuras de control, operadores (incluyendo cortocircuito de `and`/`or`, módulo, exponenciación y concatenación `@`), funciones predefinidas, construcción de objetos, despacho de métodos, `base()`, `is`/`as` están implementados. El flag `--interpret` activa esta vía.

La iteración sobre rangos la proveen `RangeValue` y `RangeIterator` en `Value/iterable.hpp`. Para iterables definidos por el usuario, el evaluador invoca `next()` y `current()` en cada paso del bucle `for`, respetando el contrato de iteración del lenguaje. Este mismo protocolo se replica en el backend LLVM, con un camino optimizado para `range` y un camino genérico basado en llamadas a métodos de instancia.

---

## 8. Generación de código

El backend LLVM (`Codegen/llvm_codegen.cpp`) traduce el AST validado a IR LLVM usando la API C++ de LLVM 21 y enlaza el resultado con un runtime mínimo en C para producir un ejecutable nativo Linux x86_64.

### 8.1 Pipeline

Tras el éxito semántico, el generador construye un módulo LLVM, emite IR a `.hulk_out.ll` e invoca `clang` con `Codegen/runtime.c` para producir `./output`. Las funciones de runtime para impresión, cadenas, objetos, comprobaciones dinámicas de tipo y fallos de cast/`case` se declaran en el IR y se definen en C.

Todos los nodos de expresión del AST tienen visitor implementado; no quedan stubs de visita.

### 8.2 Estrategia de bajada

Los literales bajan a valores LLVM del tipo apropiado. Los enlaces locales usan `alloca`/`store`/`load` con mapas de ámbito anidados. Las estructuras de control bajan a bloques básicos con puntos de fusión explícitos; las condiciones booleanas se evalúan a `i1`. Las funciones de usuario se convierten en `@hulk_fn_<nombre>_<aridad>`. Las llamadas a `print`, funciones trigonométricas, `log`, `sqrt`, `exp` y `range` bajan a llamadas al runtime en C.

### 8.3 Código orientado a objetos

Cada clase se convierte en una estructura LLVM `HulkInstance_ClassName` con etiqueta de tipo en tiempo de ejecución (`__hulk_rt_type__` como puntero a cadena) y campos almacenados como valores en caja o ranuras tipadas según la declaración. Las estructuras de subclase extienden el layout del padre. `new` asigna en el heap, inicializa la etiqueta, evalúa argumentos del constructor y ejecuta inicializadores de atributos. El acceso a atributos usa `getelementptr` sobre el layout de la estructura cuando se conoce el tipo estático.

El despacho de métodos resuelve el nombre de tipo en runtime y ramifica a la función `@hulk_meth_*` correcta, soportando sobrescritura a lo largo de cadenas de herencia sin tablas virtuales. `base()` delega en la implementación del método de la clase padre operando sobre la porción padre de `self`. `is` baja a una comparación de tipo en runtime; `as` emite un cast protegido que invoca un auxiliar de error si el cast falla. La igualdad entre tipos heterogéneos usa `hulk_boxed_equals` del runtime para que `==` se comporte de forma consistente entre primitivos y objetos.

### 8.4 Representación de valores y coerción

El generador mantiene una capa de valores unificada para transiciones entre primitivos LLVM y representación en caja en fronteras polimórficas (retornos multi-tipo, ramas de `if`/`case`, parámetros de `print`). La asignación destructiva aplica coerción cuando el valor asignado conforma al tipo del destino.

---

## 9. Sistema de compilación

El `Makefile` provee los objetivos principales:

| Objetivo | Propósito |
|----------|-----------|
| `make build` | Produce `./hulk` en la raíz del repositorio |
| `make test_semantic` | Pruebas del análisis semántico |
| `make test_eval` | Pruebas del intérprete |
| `make test_llvm` | Pruebas de humo de codegen |
| `make test_llvm_fixtures` | Compara salida nativa con intérprete |

LLVM se descubre mediante `llvm-config`. Con LLVM 21 instalado, la compilación define `HULK_HAVE_LLVM` y enlaza contra las bibliotecas correspondientes. Componentes requeridos: g++ C++17, Flex++, Make, LLVM 21 y Clang 21.

---

## 10. Estrategia de pruebas

El criterio principal de validación para codegen es la equivalencia de comportamiento:

```
./hulk programa.hulk && ./output   ≡   ./hulk programa.hulk --interpret
```

Las suites cubren análisis semántico (`tests/semantic/`), evaluación (`tests/eval/`), codegen LLVM (`tests/llvm/`), extensiones de control de flujo (`tests/extensions/`) y programas OO (`tests/matcom/oop/`). Los fixtures de codegen compilan a código nativo y comparan la salida estándar con la del intérprete como oráculo. Pruebas de humo bajo `Codegen/tests/` y `SemanticCheck/tests/` ejercitan fases aisladas. La suite `tests/multi_phase/` valida el reporte multi-fase y el modo `--first-phase`.

---

## 11. Decisiones de diseño

1. **LL(1) frente a LALR.** Ubicaciones de error predecibles, tabla visible y aplicación directa de la teoría del curso.

2. **Flex para el análisis léxico.** Herramienta estándar que refuerza el modelo de lenguajes regulares.

3. **Gramática en archivo separado.** `grammar.ll1` es la única fuente de verdad de la sintaxis.

4. **Nodos AST OO unificados.** `GetAttrExpr` + `CallExpr` / `AssignExpr` reduce tipos de nodo y simplifica visitors.

5. **CLI de doble modo.** Un binario sirve para compilación de producción y depuración.

6. **Intérprete para validación.** Comparar salida nativa con interpretación directa detecta regresiones sin mantener oráculos estáticos extensos.

7. **LLVM como backend.** IR bien tipado con generación nativa mediante Clang.

8. **Runtime en C.** E/S, cadenas y primitivas OO en `runtime.c`; el visitor LLVM se centra en bajar construcciones HULK.

9. **Despacho por tipo en runtime.** Etiqueta de tipo en cada instancia y ramificación en sitios de llamada, evitando vtables.

10. **Inferencia multipasada.** Punto fijo sobre firmas mutuamente recursivas y tipos inferidos.

---

## 12. Cumplimiento del contrato de entrega

El compilador satisface el [contrato de interfaz del curso](https://github.com/matcom/compilers/blob/main/docs/interface.md):

| Requisito | Implementación |
|-----------|----------------|
| `make build` produce `./hulk` | Compilado desde fuentes en Ubuntu LTS |
| `./hulk archivo.hulk` con éxito | Genera `./output` (ELF Linux x86_64) |
| Error léxico | Salida 1, `(línea,col) LEXICAL: mensaje` |
| Error sintáctico | Salida 2, `(línea,col) SYNTACTIC: mensaje` |
| Error semántico | Salida 3, `(línea,col) SEMANTIC: mensaje` |
| Prioridad de fases | Léxico > sintáctico > semántico |
| `REPORT.md` en la raíz | Este documento |

Ejemplo de formato de error:

```
(1,9) LEXICAL: unexpected character '$'
```

---

## 13. Limitaciones y trabajo futuro

1. **Características no implementadas.** Protocolos, vectores, lambdas y macros no forman parte del compilador actual.

2. **Idioma de los diagnósticos.** Los mensajes están en español; el prefijo `TYPE` (`LEXICAL`, `SYNTACTIC`, `SEMANTIC`) sigue el formato requerido en inglés.

3. **Optimización de llamada en cola.** Las funciones recursivas son correctas pero no optimizadas.

4. **Plataforma objetivo.** El pipeline de producción apunta a Linux x86_64.

5. **Errores en tiempo de ejecución.** Los fallos durante la ejecución (`as` inválido, `case` sin rama) los reporta `./output` con formato `(línea,col) RUNTIME: …`.

6. **Formato de impresión.** Pueden aparecer diferencias menores de formato entre intérprete y runtime nativo en ciertos escenarios de impresión de cadenas.

---

## 14. Conclusiones

Hemos construido un compilador HULK completo que implementa análisis léxico con Flex, parseo LL(1) con generación automática de tabla de análisis, un analizador semántico con inferencia de tipos multipasada y soporte orientado a objetos, cuatro extensiones de control de flujo e iteración, un intérprete de recorrido del árbol y un generador de código basado en LLVM que produce ejecutables nativos.

Las decisiones arquitectónicas clave — separar la gramática del parser, unificar construcciones OO en el AST, usar despacho por tipo en runtime y validar el código nativo contra el intérprete — mantuvieron manejable la implementación mientras soportan el poder expresivo del lenguaje, incluyendo herencia, polimorfismo, pruebas de tipo y asignación destructiva.

El compilador se construye desde fuentes con herramientas estándar de código abierto, produce diagnósticos con el formato correcto y genera binarios `./output` cuyo comportamiento coincide con la interpretación directa en las suites de prueba del proyecto.

---

## Apéndice A — Glosario

| Término | Significado |
|---------|-------------|
| CST | Árbol de sintaxis concreto — reflejo directo de las derivaciones gramaticales |
| AST | Árbol de sintaxis abstracto — árbol simplificado para semántica y codegen |
| LL(1) | Lectura izquierda a derecha, derivación por la izquierda, un token de anticipación |
| FIRST / FOLLOW | Conjuntos clásicos usados para construir la tabla de análisis LL(1) |
| Valor en caja (*boxed*) | Representación en runtime que permite comparación uniforme entre tipos |

---

## Apéndice B — Reproducir la compilación

```bash
# Instalar toolchain (Ubuntu)
sudo apt update
sudo apt install -y build-essential flex clang-21 llvm-21-dev
export LLVM_CONFIG=llvm-config-21
export PATH=/usr/lib/llvm-21/bin:$PATH

# Compilar
make build

# Ejecutar pruebas
make test_semantic
make test_eval && make test_eval_fixtures
make test_llvm && make test_llvm_fixtures

# Compilar y ejecutar un programa
./hulk playground/hello.hulk && ./output
```
