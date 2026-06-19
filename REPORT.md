# Compilador HULK — Informe de arquitectura

**Daniela De La Caridad Guerrero Álvarez · Rubén Martínez Rojas · Sammy Raul Sosa Justiz**  
Facultad de Matemática y Computación — Universidad de La Habana · 2025–2026

---

## 1. Introducción

HULK (Havana University Language for Kompilers) es un lenguaje de programación académico diseñado para el curso de Compilación de la Universidad de La Habana. El objetivo central de este proyecto es construir un compilador funcional y completo que recorra todas las fases clásicas de la teoría de compiladores: análisis léxico, análisis sintáctico, análisis semántico y generación de código o interpretación directa.

HULK es un lenguaje orientado a expresiones. Los programas consisten en cero o más declaraciones de nivel superior (funciones globales y definiciones de tipos) seguidas de una o más expresiones ejecutables. El lenguaje soporta operadores aritméticos, lógicos y de comparación; estructuras de control como `if`/`elif`/`else`, `while`, `for`, `unless`, `repeat` y `loop-while`; declaración de funciones con tipo de retorno opcional; tipos orientados a objetos con herencia simple; expresiones de ligadura con `let … in`; bloques de expresiones; pruebas de tipo con `is` y conversiones con `as`; y asignación destructiva con `:=`. Funciones predefinidas como `print`, `sin`, `cos` y `range` forman parte de la biblioteca estándar.

Este informe describe la arquitectura de nuestro compilador HULK, las decisiones de diseño tomadas en cada etapa, las funcionalidades implementadas, las limitaciones conocidas y la metodología de pruebas empleada para validar la corrección.

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

El binario del compilador `./hulk` soporta dos modos de operación dentro de un mismo ejecutable. Invocado sin flags como `./hulk archivo.hulk`, ejecuta el pipeline de producción: analiza el programa y, si no hay errores, emite un ejecutable nativo `./output` en el directorio actual. Cuando se suministran flags de desarrollo (`--interpret`, `--semantic`, `--tokens`, `--ast`), se produce salida diagnóstica adicional sin alterar el comportamiento por defecto.

Este diseño de doble modo evita mantener binarios separados para desarrollo y entrega, manteniendo simple y predecible la invocación de producción.

---

## 3. Análisis léxico

### 3.1 Función del lexer

El lexer (escáner) es la primera etapa del compilador. Lee el archivo fuente como una secuencia plana de caracteres y la transforma en una secuencia de **tokens** — pares formados por un tipo de token y, cuando aplica, un valor semántico. Por ejemplo, el texto `42` se convierte en un token `NUMBER_LITERAL` con valor numérico 42; `if` se convierte en `KEYWORD_IF`; y `mensaje` se convierte en `IDENTIFIER` con lexema `"mensaje"`.

Además de tokenizar, el lexer mantiene la posición actual en el archivo (número de línea y columna), descarta el ruido léxico (espacios en blanco y comentarios) y reporta errores léxicos ante caracteres no reconocidos o construcciones sin terminar, como cadenas y comentarios de bloque.

### 3.2 Elección de Flex

El lexer de HULK se implementó con **Flex** (Fast Lexical Analyzer Generator). Flex genera un escáner en C++ a partir de un archivo de especificación (`Lexer/hulk_lexer.l`) donde cada token se describe mediante una expresión regular y una acción asociada. Este enfoque alinea la implementación con el modelo formal de lenguajes regulares, que es el marco teórico correcto para el análisis léxico.

Flex aplica la política de **coincidencia más larga** (*longest match*): cuando varias reglas pueden hacer match, selecciona la que consume más caracteres. Esto resuelve automáticamente ambigüedades como distinguir `:=` de `:` seguido de `=`. Cuando dos reglas coinciden en longitud, Flex da prioridad a la regla que aparece primero en el archivo, lo que permite controlar la precedencia entre palabras clave e identificadores.

### 3.3 Estructura de hulk_lexer.l

La especificación del lexer sigue el diseño estándar de tres secciones de Flex, separadas por marcadores `%%`.

**Opciones de configuración.** Cuatro opciones controlan la generación de código:

- `%option yyclass="HulkLexer"` — genera `yylex()` como método de instancia de la clase `HulkLexer`, permitiendo uso orientado a objetos con un stream de entrada específico.
- `%option noyywrap` — suprime la llamada a `yywrap()` al final del archivo, ya que HULK siempre procesa un solo archivo.
- `%option never-interactive` — habilita entrada con buffer para mayor eficiencia.
- `%option nodefault` — desactiva la acción por defecto silenciosa de Flex; los caracteres no reconocidos se manejan explícitamente con una regla de respaldo `.` que reporta un error léxico.

**Macros de patrones.** Alias reutilizables de expresiones regulares mejoran la legibilidad:

```
DIGIT    [0-9]
ALPHA    [a-zA-Z]
ALNUM_   [a-zA-Z0-9_]
INT      {DIGIT}+
FLOAT    {DIGIT}+\.{DIGIT}+
IDENT    {ALPHA}{ALNUM_}*
```

**La macro UPD.** Para evitar repetir la lógica de actualización de posición y retorno en cada regla de token simple, se define una macro auxiliar en el prólogo:

```cpp
#define UPD(t) do { column_ += yyleng; \
                    return static_cast<int>(TokenType::t); } while(0)
```

El envoltorio `do { … } while(0)` es un idiom clásico de C que garantiza que la macro se comporte como una sola sentencia en cualquier contexto de control de flujo.

### 3.4 Reglas de tokens

Las reglas siguen la coincidencia más larga y la precedencia por orden de declaración:

- Los **espacios en blanco** se consumen sin emitir token; los saltos de línea incrementan el contador de línea y reinician la columna.
- Los **comentarios de línea** (`//…`) se descartan. Los **comentarios de bloque** (`/*…*/`) se manejan con un bucle manual que rastrea la posición y reporta error si no se encuentra el delimitador de cierre.
- Los **literales de cadena** acumulan caracteres entre comillas dobles, interpretan secuencias de escape (`\n`, `\t`, etc.) y reportan errores en cadenas sin terminar.
- Los **literales numéricos** reconocen flotantes antes que enteros para que el punto decimal tenga prioridad.
- Los **literales booleanos** `true` y `false` se reconocen antes que la regla general de identificadores.
- Cada **palabra clave** tiene una regla dedicada ubicada antes de los identificadores.
- Los **operadores de varios caracteres** (`:=`, `==`, `<=`, `>=`, …) se definen antes que los de un solo carácter.
- El **fin de archivo** (`<<EOF>>`) y la regla de respaldo `.` completan la especificación.

### 3.5 Valores semánticos y seguimiento de posición

Cada token lleva una estructura `SemanticValue` con tres campos: `str_val` (lexemas de identificadores y contenido de cadenas), `num_val` (literales numéricos) y `bool_val` (literales booleanos). Los campos de instancia `line_` y `column_` rastrean la posición; un adaptador copia estos valores en la estructura `Token` del parser para que todos los mensajes de error posteriores incluyan ubicaciones precisas en el fuente.

Cuando el lexer encuentra un carácter no reconocido, produce un token de tipo `UNKNOWN`. El pipeline del compilador convierte el primero de tales tokens en un diagnóstico de la forma `(línea,col) LEXICAL: unexpected character 'X'` en la salida de error estándar, con código de salida 1.

---

## 4. Análisis sintáctico

### 4.1 Función del parser

El parser toma la secuencia de tokens del lexer y verifica que cumple la gramática de HULK. En caso de éxito, construye dos representaciones en árbol:

- El **árbol de sintaxis concreto (CST)** refleja la estructura exacta de las derivaciones gramaticales, incluyendo no terminales auxiliares introducidos para compatibilidad LL(1).
- El **árbol de sintaxis abstracto (AST)** es una forma simplificada que elimina el andamiaje gramatical y conserva solo la estructura semánticamente relevante para las fases posteriores.

### 4.2 Elección del análisis LL(1)

Implementamos un **parser LL(1) dirigido por tabla**: lee la entrada de izquierda a derecha, produce una derivación por la izquierda y usa un token de anticipación (*lookahead*) para seleccionar producciones. Esta elección se motivó por varios factores.

En primer lugar, LL(1) conecta directamente con los conceptos teóricos del curso — conjuntos FIRST y FOLLOW, tablas de análisis predictivo y gramáticas libres de conflictos — y desarrollamos nuestro propio generador de tablas en lugar de depender de una herramienta de caja negra. En segundo lugar, separar la definición de la gramática (`Parser/grammar/grammar.ll1`) del algoritmo de parseo hace explícita e independientemente inspeccionable la especificación del lenguaje. En tercer lugar, los parsers LL(1) producen mensajes de error predecibles y localizados porque las decisiones se toman en cada posición de la entrada sin retroceso.

La restricción principal es que la gramática debe estar libre de recursión izquierda y de conflictos LL(1), lo que exigió reestructurar cuidadosamente la precedencia de operadores y la sintaxis del cuerpo de clase.

### 4.3 Infraestructura del parser

**Token y adaptador.** La salida del lexer se convierte mediante un adaptador de tokens (`Parser/core/token_adapter.*`) en una estructura uniforme `Token` con campos `type`, `lexeme`, `line` y `col`. La lista completa se envuelve en un `TokenStream` que provee `current()`, `advance()`, `peek(n)` y `consume(type, msg)` — este último lanza un `ParseError` con información de posición cuando falta el token esperado.

**ParseError.** Los errores sintácticos se representan como excepciones que portan el token problemático y un mensaje descriptivo. El punto de entrada las captura y las formatea como `(línea,col) SYNTACTIC: …` con código de salida 2.

### 4.4 La gramática

La gramática formal reside en `Parser/grammar/grammar.ll1`. El parser no tiene conocimiento fijo de la sintaxis de HULK; lee este archivo y construye la tabla LL(1) dinámicamente.

**Precedencia de operadores.** Fue necesario eliminar la recursión izquierda. La suma y la resta usan el patrón estándar de cola:

```
AddExpr     -> MulExpr AddExprTail
AddExprTail -> PLUS  MulExpr AddExprTail
             | MINUS MulExpr AddExprTail
             | epsilon
```

Esto produce asociatividad izquierda. La exponenciación, que requiere asociatividad derecha, usa una cola recursiva a la derecha:

```
PowerExpr     -> UnaryExpr PowerExprTail
PowerExprTail -> CARET PowerExpr
               | epsilon
```

de modo que `2^3^2` se parsea como `2^(3^2)`.

**Desambiguación del cuerpo de clase.** Dentro de una declaración de clase, tanto atributos como métodos comienzan con un identificador. Con un solo token de anticipación, el parser no puede distinguirlos de inmediato. Aplicamos factorización por la izquierda: se consume el identificador y luego se inspecciona el token siguiente — `COLON` indica un atributo, `LPAREN` un método. Esto se implementa mediante el no terminal `ClassAttrListHead`.

**Sin punto y coma tras la llave de cierre de clase.** Las declaraciones de clase no requieren punto y coma después de `}`. Añadir un `;` opcional crearía un conflicto LL(1): tras `}`, el parser no podría determinar si `;` pertenece a la clase o a la siguiente sentencia. Omitirlo mantiene la gramática sin ambigüedad y coincide con la especificación del lenguaje HULK.

**Restricción de bloques.** La gramática define que `FIRST(Expr)` no incluye `LBRACE`, por lo que los bloques `{…}` aparecen solo en posiciones explícitamente permitidas (cuerpos de funciones, de `let`/`if`/`while`, etc.) y no pueden usarse como operandos arbitrarios.

### 4.5 Generación de la tabla LL(1)

El generador (`Parser/generator/`) comprende tres componentes:

1. **Lector de gramática** — parsea `grammar.ll1` en objetos de producción e identifica terminales, no terminales y símbolo inicial.
2. **Calculador FIRST/FOLLOW** — computa ambos conjuntos iterativamente hasta un punto fijo con los algoritmos clásicos.
3. **Constructor de tabla** — construye `M[A, a]`; si una segunda producción se asigna a una celda ya ocupada, se registra un conflicto LL(1). Nuestra gramática actual tiene cero conflictos, verificado por una prueba de humo automatizada.

### 4.6 El parser predictivo y el CST

El parser LL(1) (`Parser/syntax/ll1_parser.*`) mantiene una pila inicializada con el símbolo inicial `Program`. En cada paso empareja un terminal con el token actual de entrada o expande un no terminal usando la entrada de tabla para el anticipación actual. El CST se construye simultáneamente: los nodos internos corresponden a no terminales y las hojas a terminales consumidos.

Se generan mensajes de error sensibles al contexto analizando el estado del parseo — por ejemplo, reportando que un operador no es aplicable a una expresión de cierto tipo en lugar de un genérico «token inesperado».

### 4.7 Conversión CST a AST

La conversión (`Parser/ast/cst_to_ast.cpp`) recorre el CST recursivamente, aplanando el ruido gramatical y construyendo un AST limpio. Bajo un nodo raíz `Program` emergen dos jerarquías:

- **Sentencias (`Stmt`)** — declaraciones de funciones, declaraciones de tipos, sentencias de expresión.
- **Expresiones (`Expr`)** — cualquier construcción que produce o evalúa un valor.

Transformaciones notables:

- **Expresiones binarias** — cadenas de cola como `AddExpr` + `AddExprTail` se aplanan en nodos `BinaryExpr` con asociatividad izquierda.
- **Cadenas postfix** — accesos y llamadas secuenciales (`obj.m(a).b`) se convierten en nodos anidados `GetAttrExpr` y `CallExpr`.
- **Ligaduras let** — `let x = 1, y = 2 in expr` se convierte en nodos `LetExpr` anidados.
- **If/elif** — las ramas elif se normalizan a nodos `IfExpr` anidados.
- **Representación OO unificada** — las llamadas a método `obj.m(a)` se convierten en `Call(GetAttr(obj, m), [a])`; la asignación a atributo `obj.attr := v` en `Assign(GetAttr(obj, attr), :=, v)`. Este diseño de dos nodos (en lugar de cuatro tipos separados) simplifica tanto el análisis semántico como la generación de código.

Los tipos de nodo del AST cubren literales, identificadores, todos los operadores binarios y unarios, `let`, bloques, `if`/`while`/`for`, declaraciones de funciones y tipos, `is`/`as`, `new` y asignación destructiva.

---

## 5. Análisis semántico

### 5.1 Estructura de dos pasadas

El análisis semántico se divide en una pasada de **recolección de declaraciones** y otra de **verificación de tipos**. Esta separación permite que las declaraciones orientadas a objetos referencien tipos registrados pero aún no analizados completamente dentro de la misma unidad de compilación.

La pasada de recolección (`decl_collector.cpp`) recorre las sentencias de nivel superior y registra firmas de funciones (con sobrecarga por aridad), nombres de clases, aristas de herencia, nombres de atributos y firmas de métodos. Las funciones predefinidas y las constantes globales (`PI`, `E`) se inyectan antes del código del usuario.

El analizador principal (`SemanticCheck/phase2_checker.cpp`) valida entonces el programa completo.

### 5.2 Reglas núcleo (R1–R4)

**R1 — Una definición por ámbito.** Un nombre de variable solo puede declararse una vez dentro del mismo ámbito `let`. El sombreado (*shadowing*) entre ámbitos anidados está permitido. La asignación destructiva `:=` reasigna un enlace existente en lugar de crear uno nuevo. Las globales predefinidas no pueden redefinirse con `let`.

**R2 — Definida antes de uso.** Toda variable debe estar definida antes de aparecer en una expresión. En `let a = 1, b = a + 1 in …`, los enlaces se procesan de izquierda a derecha de modo que `b` puede referenciar `a`. La autorreferencia en un inicializador (`let x = x in …`) se rechaza. Las funciones son solo globales y no capturan variables de ámbitos `let` exteriores.

**R3 — Parámetros únicos y sobrecarga.** Los nombres de parámetros deben ser únicos dentro de la firma de una función o método. Varias funciones globales pueden compartir nombre si difieren en aridad; redefinir el mismo nombre con la misma aridad es un error.

**R4 — Orden textual de declaración.** Las funciones se registran al encontrar sus declaraciones. Una llamada a una función aún no declarada en el fuente produce `UNDEFINED_FUNCTION`. Las funciones predefinidas están precargadas y no pueden redeclararse.

### 5.3 Semántica orientada a objetos

Para cada declaración `type`, el analizador valida que los objetivos de `inherits` existen, detecta ciclos de herencia, registra atributos en el ámbito de la clase y analiza cuerpos de métodos en un ámbito donde `self` tiene el tipo de la clase actual. Las comprobaciones de compatibilidad de sobrescritura de métodos comparan tipos de retorno y listas de parámetros con los métodos del padre a lo largo de la cadena de herencia.

La expresión `new` se valida contra la lista de parámetros del constructor del tipo objetivo. Las expresiones de inicialización de atributos se verifican en el contexto de la clase. Las llamadas a `base()` están permitidas solo dentro de métodos de clases con padre.

### 5.4 Inferencia de tipos

HULK permite omitir anotaciones de tipo en varias posiciones: variables locales en `let`, tipos de retorno de funciones y métodos, y tipos de atributos. El analizador ejecuta hasta diez pasadas de punto fijo sobre cuerpos de funciones y métodos hasta que los tipos se estabilizan o se reporta un error. Los operadores binarios numéricos, de cadena y booleanos tienen reglas de propagación que asignan tipos a referencias de identificadores sin tipo cuando se conoce el tipo del otro operando.

### 5.5 Sistema de tipos

`Types/type_info.cpp` modela `Number`, `String`, `Boolean`, `Null`, tipos objeto y tipos desconocidos o inferidos. El subtipado a lo largo de aristas de herencia se verifica mediante `conformsTo`. El operador `is` produce un `Boolean`; `as` produce el tipo objetivo cuando el cast es estáticamente posible, con comprobaciones en tiempo de ejecución emitidas por el generador de código para los casos dinámicos.

### 5.6 Reporte de errores

Los errores semánticos se recogen en un `ErrorManager` y se reportan como `(línea,col) SEMANTIC: mensaje` en la salida de error estándar con código de salida 3. Los diagnósticos idénticos duplicados se colapsan antes de la salida. El modo desarrollo puede listar todos los errores semánticos en una sola ejecución; el pipeline de producción reporta errores solo de la primera fase que falla (los léxicos tienen prioridad sobre los sintácticos, y estos sobre los semánticos).

---

## 6. Intérprete

Junto con la generación de código, el compilador incluye un intérprete de recorrido del árbol (`Evaluator/evaluator.cpp`) que ejecuta el AST directamente. Este componente sirve para desarrollo y validación: su salida en la salida estándar se compara con el binario nativo `./output` para verificar que ambas vías de ejecución producen resultados idénticos.

### 6.1 Modelo de valores en tiempo de ejecución

El módulo `Value/` define un tipo `Value` con etiquetas que soporta `Number`, `String`, `Boolean`, `Null`, `Range` e instancias objeto `Instance`. Las instancias objeto portan un puntero a su declaración de clase y un entorno de atributos. La iteración sobre rangos la proveen `RangeValue` y `RangeIterator` en `Value/iterable.hpp`.

### 6.2 Entorno de ejecución

Los enlaces se gestionan mediante una cadena de marcos de entorno (`Evaluator/env_frame.hpp`). La búsqueda para asignación destructiva `:=` recorre marcos ancestros de modo que la reasignación alcance el enlace correcto incluso en ámbitos anidados. Las funciones definidas por el usuario se almacenan en un mapa indexado por nombre y aridad (`Evaluator/user_function.hpp`).

### 6.3 Ejecución del programa

El evaluador procesa un `Program` en dos pasadas: primero registra todas las declaraciones de tipo para que las expresiones `new` puedan resolver metadatos de clase, luego ejecuta funciones de nivel superior y la expresión final del programa. Todas las estructuras de control, operadores (incluyendo cortocircuito de `and`/`or`, módulo, exponenciación y concatenación de cadenas con `@`), funciones predefinidas, construcción de objetos, despacho de métodos, llamadas a `base()` e `is`/`as` están implementados en el visitor.

El flag `--interpret` activa esta vía e imprime solo la salida del programa en la salida estándar, manteniendo limpia la salida de error para diagnósticos.

---

## 7. Generación de código

El backend LLVM (`Codegen/llvm_codegen.cpp`) traduce el AST validado a IR LLVM usando la API C++ de LLVM 21 y enlaza el resultado con un runtime mínimo en C para producir un ejecutable nativo Linux x86_64.

### 7.1 Pipeline

Tras el éxito del análisis semántico, el generador de código construye un módulo LLVM, emite IR a `.hulk_out.ll` e invoca `clang` con `Codegen/runtime.c` para producir `./output`. Las funciones de runtime para impresión, manipulación de cadenas, asignación de objetos, comprobaciones dinámicas de tipo e igualdad en cajas (*boxed*) se declaran en el módulo IR y se definen en el runtime en C.

### 7.2 Estrategia de bajada (*lowering*)

**Literales y globales.** Los literales numéricos, de cadena y booleanos bajan a valores LLVM del tipo apropiado. Las constantes matemáticas `@PI` y `@E` se emiten como variables globales.

**Variables y ámbitos.** Los enlaces locales usan `alloca`/`store`/`load` con mapas de ámbito anidados que rastrean los valores LLVM asociados a cada nombre. El sombreado crea un nuevo enlace en el ámbito interior sin destruir los del exterior.

**Flujo de control.** `if` y `while` bajan a bloques básicos con puntos de fusión explícitos. Las condiciones booleanas se evalúan a `i1`. Los operadores lógicos de cortocircuito se manejan en el visitor del AST antes del bajado a LLVM.

**Funciones.** Las funciones de usuario se convierten en funciones LLVM nombradas `@hulk_fn_<nombre>_<aridad>`. La recursión está soportada sin optimización especial de llamada en cola. Las rutas de retorno dejan un valor en la pila o usan `ret void` para cuerpos que son solo sentencias.

**Funciones predefinidas.** Las llamadas a `print`, funciones trigonométricas, `log`, `sqrt`, `exp` y `range` bajan a llamadas a las funciones correspondientes del runtime en C. El bucle `for` sobre un rango se desazucara en un protocolo de iteración usando la implementación de rangos del runtime.

### 7.3 Generación de código orientada a objetos

**Layout de instancia.** Cada clase se convierte en una estructura LLVM `HulkInstance_ClassName` que contiene una etiqueta de tipo en tiempo de ejecución (`__hulk_rt_type__` como puntero a cadena) y campos almacenados como valores en caja o ranuras tipadas según la declaración. Las estructuras de subclase embeben o extienden el layout del padre.

**Construcción.** `new NombreTipo(…)` asigna una estructura en el heap, inicializa la etiqueta de tipo, evalúa argumentos del constructor y ejecuta expresiones inicializadoras de atributos.

**Despacho de métodos.** En lugar de tablas virtuales, las llamadas a método resuelven el nombre de tipo en tiempo de ejecución y ramifican a la función `@hulk_meth_*` correcta. Este diseño reduce la complejidad del codegen mientras soporta sobrescritura de métodos a lo largo de cadenas de herencia.

**Herencia.** Las llamadas a `base()` delegan en la implementación del método de la clase padre operando sobre la porción padre de `self`. El acceso a atributos usa `getelementptr` sobre el layout de la estructura cuando se conoce el tipo estático.

**Pruebas y conversiones de tipo.** El operador `is` baja a una comparación de tipo en tiempo de ejecución. El operador `as` emite un cast protegido; un cast fallido invoca un auxiliar de error en tiempo de ejecución (fallo de runtime, no diagnóstico de compilación).

**Igualdad.** Las comparaciones entre tipos usan una función de runtime `hulk_boxed_equals` para que `==` se comporte de forma consistente entre primitivos y objetos.

### 7.4 Detalles de implementación

`LLVM Function::Create` requiere nombres estables `const char*`; el generador almacena cadenas de nombre en contenedores `std::string` antes de pasar `.c_str()` para evitar punteros colgantes. La serialización del IR usa un buffer `SmallString` vía `raw_svector_ostream` en lugar de imprimir directamente a `std::string`.

Las definiciones anidadas de funciones y métodos mantienen mapas de ámbito redimensionando a la profundidad correcta en lugar de limpiar por completo, evitando uso después de liberación cuando el constructor de IR mantiene punteros a estructuras de ámbito.

---

## 8. Sistema de compilación

El `Makefile` provee los siguientes objetivos principales:

| Objetivo | Propósito |
|----------|-----------|
| `make compile` | Compila el binario del compilador con todas las fuentes |
| `make build` | Produce `./hulk` en la raíz del repositorio (compilado con `-O2` en Linux) |
| `make test_llvm` | Ejecuta pruebas de humo de codegen |
| `make test_llvm_fixtures` | Compara la salida del binario nativo con la del intérprete |
| `make test_eval` | Ejecuta pruebas de humo del intérprete |
| `make test_semantic` | Ejecuta pruebas de humo del análisis semántico |

LLVM se descubre mediante `llvm-config` en el path del sistema. Cuando están instalados los paquetes de desarrollo de LLVM 21, la compilación define `HULK_HAVE_LLVM` y enlaza contra las bibliotecas LLVM. La variable de entorno `LLVM_CONFIG` puede sobreescribir el nombre del binario por defecto (p. ej. `llvm-config-21`).

Componentes requeridos del toolchain: compilador C++17 (`g++`), Flex++ (`flex++`), Make, LLVM 21 (`llvm-config`, cabeceras, bibliotecas) y Clang 21 para el paso final de enlace.

---

## 9. Estrategia de pruebas

### 9.1 Criterio de corrección del código nativo

La regla principal de validación para la generación de código es la equivalencia de comportamiento:

```
./hulk programa.hulk && ./output   debe producir la misma stdout que   ./hulk programa.hulk --interpret
```

Esto se verifica mediante `scripts/test_llvm_fixtures.sh` sobre la suite de pruebas LLVM y programas orientados a objetos.

### 9.2 Suites de prueba

**Fixtures semánticos** (`tests/semantic/`) — programas válidos e inválidos que ejercitan las reglas R1–R4, inferencia de tipos, construcciones OO y resolución de funciones predefinidas.

**Fixtures de evaluación** (`tests/eval/`) — programas ejecutados por el intérprete, cubriendo operadores, flujo de control, funciones, predefinidas y comportamiento OO completo incluyendo herencia, `base()`, `is` y `as`.

**Fixtures de codegen** (`tests/llvm/`) — programas compilados a código nativo y comparados con la salida del intérprete.

**Programas orientados a objetos** — suite dedicada que cubre clases básicas, herencia, sobrescritura de métodos, expresiones de constructor y despacho polimórfico.

**Pruebas de humo** — drivers C++ independientes bajo `Codegen/tests/` y `SemanticCheck/tests/` que construyen fragmentos mínimos de AST o ejecutan fases aisladas del compilador sin requerir un parseo completo.

### 9.3 Pruebas de errores

Pruebas dedicadas de errores léxicos verifican que caracteres inválidos, cadenas sin terminar y comentarios de bloque sin cerrar producen diagnósticos `LEXICAL` con código de salida 1. Los fixtures de errores de parser y semántica verifican el comportamiento `SYNTACTIC` (salida 2) y `SEMANTIC` (salida 3) respectivamente.

---

## 10. Decisiones de diseño

1. **LL(1) frente a LALR.** Ubicaciones de error predecibles, tabla de parseo visible y depurable, y aplicación directa de la teoría del curso. El tamaño de la gramática es manejable sin la complejidad añadida de un generador bottom-up.

2. **Flex para el análisis léxico.** Herramienta estándar que refuerza el modelo de lenguajes regulares y resuelve automáticamente la desambiguación por coincidencia más larga.

3. **Gramática en archivo separado.** `grammar.ll1` es la única fuente de verdad de la sintaxis; el motor del parser es agnóstico a la gramática.

4. **Nodos AST OO unificados.** Representar llamadas a método y acceso a atributos mediante `GetAttrExpr` + `CallExpr` / `AssignExpr` reduce el número de tipos de nodo y simplifica todos los visitors posteriores.

5. **CLI de doble modo.** Un solo binario sirve tanto para compilación de producción como para depuración en desarrollo sin contaminar la ruta por defecto con flags.

6. **Intérprete para validación.** Comparar la salida nativa con la interpretación directa evita mantener grandes colecciones de archivos de salida esperada y detecta regresiones de codegen temprano.

7. **LLVM como backend.** El IR LLVM ofrece una representación intermedia bien tipada y portable, con optimización madura y generación de código nativo mediante Clang.

8. **Runtime en C.** Mantener E/S, manejo de cadenas y primitivas de objetos en `runtime.c` permite que el visitor LLVM se centre en bajar construcciones HULK en lugar de detalles de libc.

9. **Despacho por tipo en runtime frente a vtables.** Almacenar una cadena con el nombre del tipo en cada instancia y ramificar en los sitios de llamada evita construcción de vtables y corrección de punteros, manteniendo polimorfismo y sobrescritura.

10. **Inferencia de tipos multipasada.** La iteración de punto fijo maneja firmas de funciones mutuamente recursivas y tipos de atributos inferidos sin exigir que el programador anote explícitamente cada enlace.

---

## 11. Cumplimiento del contrato de entrega

El compilador satisface el contrato de interfaz del curso:

| Requisito | Implementación |
|-----------|----------------|
| `make build` produce `./hulk` | Compilado desde fuentes en Ubuntu LTS |
| `./hulk archivo.hulk` con éxito | Genera `./output` (ELF Linux x86_64) |
| Error léxico | Salida 1, `(línea,col) LEXICAL: mensaje` |
| Error sintáctico | Salida 2, `(línea,col) SYNTACTIC: mensaje` |
| Error semántico | Salida 3, `(línea,col) SEMANTIC: mensaje` |
| Prioridad de fases | Léxico > sintáctico > semántico |

Ejemplo:

```
(1,9) LEXICAL: unexpected character '$'
```

Validado en Ubuntu 22.04 con g++ 11.4, LLVM 21.1.8 y Clang 21.

---

## 12. Limitaciones y trabajo futuro

1. **Extensiones del lenguaje.** Arreglos, macros, expresiones lambda, interfaces y generadores no están implementados.

2. **Idioma de los diagnósticos.** Los mensajes de error están redactados en español, mientras que el prefijo `TYPE` (`LEXICAL`, `SYNTACTIC`, `SEMANTIC`) sigue el formato requerido en inglés.

3. **Optimización de llamada en cola.** Las funciones recursivas son correctas pero no optimizadas.

4. **Salida multiplataforma.** El pipeline de producción apunta a Linux x86_64. El desarrollo en Windows usa el intérprete y una copia del binario; la generación nativa de `./output` se valida en Linux.

5. **Errores en tiempo de ejecución.** Los fallos durante la ejecución del programa (como un cast `as` fallido) los reporta `./output` en runtime y quedan fuera del alcance de los diagnósticos estáticos del compilador.

6. **Formato de impresión de cadenas.** Pueden aparecer diferencias menores de comillas o formato entre el intérprete y el runtime nativo en ciertos escenarios de impresión de cadenas.

---

## 13. Conclusiones

Hemos construido un compilador HULK completo que implementa análisis léxico con Flex, parseo LL(1) dirigido por tabla con generación automática de conjuntos FIRST/FOLLOW y tabla de análisis, un analizador semántico rico con inferencia de tipos multipasada y soporte orientado a objetos completo, un intérprete de recorrido del árbol y un generador de código basado en LLVM que produce ejecutables nativos.

Las decisiones arquitectónicas clave — separar la gramática del parser, unificar construcciones OO en el AST, usar despacho por tipo en runtime en lugar de vtables y validar el código nativo contra el intérprete — mantuvieron manejable la implementación mientras soportan el poder expresivo completo del lenguaje HULK, incluyendo herencia, polimorfismo, pruebas de tipo y asignación destructiva.

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
| SSA | Asignación estática única — propiedad del IR LLVM donde cada registro se asigna una vez |

---

## Apéndice B — Reproducir la compilación

```bash
# Instalar toolchain (Ubuntu)
sudo apt update
sudo apt install -y build-essential flex clang-21 llvm-21-dev
export LLVM_CONFIG=llvm-config-21
export PATH=/usr/lib/llvm-21/bin:$PATH

# Compilar
make compile && make build

# Ejecutar pruebas
make test_semantic
make test_eval && make test_eval_fixtures
make test_llvm && make test_llvm_fixtures

# Compilar y ejecutar un programa
./hulk playground/hello.hulk && ./output
```
