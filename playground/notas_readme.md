# Notas README — dudas y respuestas

## 3.3 Estructura de hulk_lexer.l

La especificación del lexer sigue el diseño estándar de tres secciones de Flex, separadas por marcadores %%.

Opciones de configuración. Cuatro opciones controlan la generación de código:

%option yyclass="HulkLexer" — genera yylex() como método de instancia de la clase HulkLexer, permitiendo uso orientado a objetos con un stream de entrada específico.
%option noyywrap — suprime la llamada a yywrap() al final del archivo, ya que HULK siempre procesa un solo archivo.
%option never-interactive — habilita entrada con buffer para mayor eficiencia.
%option nodefault — desactiva la acción por defecto silenciosa de Flex; los caracteres no reconocidos se manejan explícitamente con una regla de respaldo . que reporta un error léxico.

explicame mejor esto que no comprendo que es genera yylex y que significa que permite uso orientado a objetos con un stream de entrada específico.

que significa que %option noyywrap — suprime la llamada a yywrap() al final del archivo, ya que HULK siempre procesa un solo archivo.


que significa que %option never-interactive — habilita entrada con buffer para mayor eficiencia.

que significa que
%option nodefault — desactiva la acción por defecto silenciosa de Flex; los caracteres no reconocidos se manejan explícitamente con una regla de respaldo . que reporta un error léxico.

### Respuesta — opciones de Flex

**`%option yyclass="HulkLexer"` — ¿qué es `yylex()` y el “uso orientado a objetos”?**

Flex genera una función llamada **`yylex()`**, que es el corazón del lexer: lee caracteres del input y devuelve el siguiente token (`PLUS`, `IDENTIFIER`, etc.).

Por defecto, `yylex()` es una función libre global. Con **`%option yyclass="HulkLexer"`**, Flex la genera como **método de la clase `HulkLexer`**. Eso implica:

- Puedes crear **varias instancias** del lexer (`HulkLexer lexer1(input1)`, `lexer2(input2)`).
- Cada instancia tiene su **propio stream de entrada** y su **propio estado** (`line_`, `column_`, `lexeme_`, etc.).
- Dentro de las reglas del `.l`, `this` apunta a tu `HulkLexer`, por eso puedes usar campos de la clase directamente.

En resumen: no es “OO” en el sentido de herencia polimórfica; es **encapsular el lexer en un objeto** para poder tokenizar distintos archivos/streams sin variables globales compartidas.

**`%option noyywrap`**

Flex, al llegar al **fin de un archivo**, llama por defecto a **`yywrap()`** para preguntar: “¿hay otro archivo que seguir leyendo?”. Si devuelve 0, continúa; si 1, termina.

HULK **siempre procesa un solo archivo**, así que no necesitas esa lógica. **`noyywrap`** hace que Flex **no llame a `yywrap()`** y termine directamente al EOF. Evitas implementar esa función y simplificas el flujo.

**`%option never-interactive`**

Flex puede operar en modo **interactivo** (esperando entrada del teclado línea a línea) o con **buffer interno** (lee bloques grandes del stream de una vez).

**`never-interactive`** fuerza el modo **bufferizado/no interactivo**: lee del `istream` en chunks, más eficiente para archivos completos. Para un compilador que lee `.hulk` enteros, es lo correcto.

**`%option nodefault`**

Por defecto, Flex tiene una regla implícita que **coincide con cualquier carácter no reconocido y lo ignora silenciosamente** (sigue escaneando sin avisar).

**`nodefault` desactiva eso.** Si ninguna regla coincide, Flex no “traga” el carácter. En HULK hay una regla explícita `.` que reporta error léxico. Así **todo carácter inválido se detecta y se reporta**, no se pierde.

---

## La macro UPD

La macro UPD. Para evitar repetir la lógica de actualización de posición y retorno en cada regla de token simple, se define una macro auxiliar en el prólogo:

que es la macro, que es upd y por que evita repetir la lógica de actualización de posición y retorno en cada regla de token simple


El envoltorio do { … } while(0) es un idiom clásico de C que garantiza que la macro se comporte como una sola sentencia en cualquier contexto de control de flujo.

que significa aqui que es un idiom clasico de C

### Respuesta — macro UPD

Una **macro** en C/C++ es texto que el preprocesador **sustituye** antes de compilar. No es una función; es un atajo.

**`UPD(t)`** (definida en `Lexer/hulk_lexer.l`) hace en cada regla de token simple:

1. **`MARK_TOKEN()`** — guarda `(line_, column_)` donde empieza el token.
2. **`SET_LEXEME()`** — guarda el texto reconocido (`yytext`).
3. **`column_ += yyleng`** — avanza la columna según la longitud del match.
4. **`return TokenType::t`** — devuelve el enum del token.

Sin la macro, cada regla repetiría esas 4 líneas. Con `"+" { UPD(PLUS); }` queda una sola línea.

**El `do { … } while(0)` — “idiom clásico de C”**

Un **idiom** es un patrón habitual en un lenguaje, no una regla del lenguaje en sí.

Las macros se expanden como texto. Si escribieras:

```c
#define UPD(t) MARK_TOKEN(); SET_LEXEME(); ...
```

y la usaras tras un `if` sin llaves:

```c
if (cond)
    UPD(PLUS);  // solo la primera línea quedaría dentro del if
else ...
```

sería un bug. **`do { … } while(0)`** envuelve la macro en **un solo bloque** que siempre se comporta como **una sentencia**, usable con `if`, `else`, etc.

---

## 4.4 — Precedencia de operadores

EN ESTA PARTE DE 4.4

Precedencia de operadores. Fue necesario eliminar la recursión izquierda. La suma y la resta usan el patrón estándar de cola:

AddExpr     -> MulExpr AddExprTail
AddExprTail -> PLUS  MulExpr AddExprTail
             | MINUS MulExpr AddExprTail
             | epsilon
Esto produce asociatividad izquierda. La exponenciación, que requiere asociatividad derecha, usa una cola recursiva a la derecha:

PowerExpr     -> UnaryExpr PowerExprTail
PowerExprTail -> CARET PowerExpr
               | epsilon
de modo que 2^3^2 se parsea como 2^(3^2).

EXPLICAME CON MAS DETALLES QUE SIGNIFICA QUEFUE NECESARIO ELIMINAR LA RECURSION IZQUIERDA, COMO SE HIZO Y UQ ESIGNIFICA QUE USA EL PATRON ESTANDAR DE COLA

QUE SIGNFIICA QUE ESTO PRODUCE ASOCIATIVIDAD IZQUIERDA

Y QUE SIGNIFICA QUE LA EXPONENCIACION REQUIERE ASOCIATIVIDAD DERECHA, Y USA UNA COLA RECURSIVA A LA DERECHA

### Respuesta — recursión izquierda y asociatividad

**¿Por qué eliminar recursión izquierda?**

Un parser **LL(1)** (como el de HULK) mira **un token de anticipación** y decide qué producción usar **sin retroceder**. Las reglas **recursivas a la izquierda** como:

```
AddExpr -> AddExpr PLUS MulExpr
```

son problemáticas: al expandir `AddExpr`, vuelves a necesitar `AddExpr` antes de consumir nada → **conflicto** en la tabla LL(1).

**Solución estándar:** reescribir en forma de **cola** (tail recursion a la derecha en la gramática):

```
AddExpr     -> MulExpr AddExprTail
AddExprTail -> PLUS MulExpr AddExprTail | MINUS MulExpr AddExprTail | ε
```

Así `AddExpr` **empieza** con algo concreto (`MulExpr`) y el resto de sumas/restas va en la cola.

**“Patrón estándar de cola”**

**Cola** = no terminal auxiliar `*Tail` que repite “operador + operando” cero o más veces, terminando en **ε** (vacío). Es el patrón clásico para operadores binarios en gramáticas LL.

**¿Por qué produce asociatividad izquierda?**

Para `1 + 2 + 3`, el CST queda algo como:

```
AddExpr
  MulExpr(1)
  AddExprTail
    PLUS MulExpr(2) AddExprTail
      PLUS MulExpr(3) AddExprTail ε
```

Al convertir a AST (`build_add_tail` en `Parser/ast/cst_to_ast.cpp`), se construye:

```
BinaryExpr( BinaryExpr(1, +, 2), +, 3 )
```

El **primer operando queda a la izquierda del árbol** → **`1 + 2` se agrupa antes que `+ 3`** → asociatividad **izquierda** (`(1+2)+3`).

**Exponenciación — asociatividad derecha y cola a la derecha**

Matemáticamente, **`^` asocia a la derecha**: `2^3^2` = `2^(3^2)` = 512, no `(2^3)^2`.

Por eso la cola es distinta:

```
PowerExprTail -> CARET PowerExpr | ε
```

Nota: el lado derecho es **`PowerExpr` completo**, no solo `UnaryExpr`. Eso hace que `3^2` se parse como un subárbol antes de unirse con el `2` de la izquierda. En `build_power_tail` se conserva la asociatividad derecha: el lado derecho vuelve a ser `PowerExpr` completo.

Resultado: `BinaryExpr(2, ^, BinaryExpr(3, ^, 2))` → **`2^(3^2)`**.

---

## 4.6 — Parser predictivo y CST

EN ESTA PARTE

4.6 El parser predictivo y el CST
El parser LL(1) (Parser/syntax/ll1_parser.*) mantiene una pila inicializada con el símbolo inicial Program. En cada paso empareja un terminal con el token actual de entrada o expande un no terminal usando la entrada de tabla para el anticipación actual. El CST se construye simultáneamente: los nodos internos corresponden a no terminales y las hojas a terminales consumidos.

Se generan mensajes de error sensibles al contexto analizando el estado del parseo — por ejemplo, reportando que un operador no es aplicable a una expresión de cierto tipo en lugar de un genérico «token inesperado».

EXPLICAME CON MAS DETALLES QUE SIGNIFICA QUE EN En cada paso empareja un terminal con el token actual de entrada o expande un no terminal usando la entrada de tabla para el anticipación actual.
Y COMO SE PUEDE REDACTAR MEJOR LA PARTE DE USANDO LA ENTRADA DE TABLA PARA EL ANTICIPACION ACTUAL, QUE ES EL ANTICIPACION ACTUAL

### Respuesta — parser LL(1)

**¿Qué hace en cada paso?**

El parser mantiene una **pila** de símbolos pendientes (inicialmente `Program`). En cada iteración mira el **tope**:

1. **Si es terminal** (ej. `PLUS`, `IDENTIFIER`): debe coincidir con el **token actual** del input. Si coincide, lo consume y lo cuelga como hoja en el CST. Si no, error sintáctico.
2. **Si es no terminal** (ej. `AddExpr`): **no consume** input todavía; **expande** según una producción de la gramática, empuja los símbolos del lado derecho en la pila (en orden inverso) y crea un nodo interno en el CST.

**Redacción más clara que “entrada de tabla para el anticipación actual”**

Podrías decir:

> “Expande el no terminal consultando la **tabla LL(1)**: fila = no terminal en la cima de la pila, columna = **token de lookahead** (el siguiente token sin consumir). La celda indica qué producción aplicar.”

**Anticipación (lookahead)** = el **próximo token real** del stream, sin haberlo consumido aún. En código: `current_terminal()` → nombre del token actual (`PLUS`, `LET`, …). La tabla responde: “viendo `PLUS`, ¿cómo expando `AddExprTail`?”.

---

## 4.7 — Conversión CST a AST

EN ESTA PARTE

4.7 Conversión CST a AST
La conversión (Parser/ast/cst_to_ast.cpp) recorre el CST recursivamente, aplanando el ruido gramatical y construyendo un AST limpio. Bajo un nodo raíz Program emergen dos jerarquías:

Sentencias (Stmt) — declaraciones de funciones, declaraciones de tipos, sentencias de expresión.
Expresiones (Expr) — cualquier construcción que produce o evalúa un valor.
Transformaciones notables:

Expresiones binarias — cadenas de cola como AddExpr + AddExprTail se aplanan en nodos BinaryExpr con asociatividad izquierda.
Cadenas postfix — accesos y llamadas secuenciales (obj.m(a).b) se convierten en nodos anidados GetAttrExpr y CallExpr.
Ligaduras let — let x = 1, y = 2 in expr se convierte en nodos LetExpr anidados.
If/elif — las ramas elif se normalizan a nodos IfExpr anidados.
Representación OO unificada — las llamadas a método obj.m(a) se convierten en Call(GetAttr(obj, m), [a]); la asignación a atributo obj.attr := v en Assign(GetAttr(obj, attr), :=, v). Este diseño de dos nodos (en lugar de cuatro tipos separados) simplifica tanto el análisis semántico como la generación de código.
Los tipos de nodo del AST cubren literales, identificadores, todos los operadores binarios y unarios, let, bloques, if/while/for, declaraciones de funciones y tipos, is/as, new y asignación destructiva.

EXPLICAME CON MAS DETALLES


Expresiones binarias — cadenas de cola como AddExpr + AddExprTail se aplanan en nodos BinaryExpr con asociatividad izquierda.
QUE SIGNIFICA QUE SE APLANAN EN NODOS CON ASOCIATIVIDAD IZQUIERDA



Cadenas postfix — accesos y llamadas secuenciales (obj.m(a).b) se convierten en nodos anidados GetAttrExpr y CallExpr.
QUE SIGNIFICA QUE SE CONVIERTEN EN NODOS ANIDADOS GetAttrExpr y CallExpr., QUE SON ESTOS NODOS Y COMO FUNCIONAN

Ligaduras let — let x = 1, y = 2 in expr se convierte en nodos LetExpr anidados.
If/elif — las ramas elif se normalizan a nodos IfExpr anidados.

QUE SON NODOS LetExpr anidados. y que son nodos IfExpr anidados.



Representación OO unificada — las llamadas a método obj.m(a) se convierten en Call(GetAttr(obj, m), [a]); la asignación a atributo obj.attr := v en Assign(GetAttr(obj, attr), :=, v). Este diseño de dos nodos (en lugar de cuatro tipos separados) simplifica tanto el análisis semántico como la generación de código.

que significa que las llamadas a método obj.m(a) se convierten en Call(GetAttr(obj, m), [a]); la asignación a atributo obj.attr := v en Assign(GetAttr(obj, attr), :=, v) 
y por que este diseño simplifica tanto el analisis semantico como la generacion de codigo

### Respuesta — CST → AST

**Expresiones binarias “aplanadas” con asociatividad izquierda**

El CST tiene **estructura de cola** (`AddExpr` + `AddExprTail` + `AddExprTail`…), ruidosa para análisis posterior. **Aplanar** = convertir esa cadena en **árbol de `BinaryExpr` anidados** con la forma correcta.

“Asociatividad izquierda en el AST” significa que el nodo resultante agrupa **`a + b` antes que `+ c`**, no al revés.

**Cadenas postfix — `GetAttrExpr` y `CallExpr` anidados**

En HULK, `obj.m(a).b` es **postfix**: primario `obj`, luego `.m`, luego `(a)`, luego `.b`.

El CST lo representa como cola `PostfixTail`. Al convertir (`build_postfix_tail`):

- `.m` → **`GetAttrExpr(obj, "m")`**
- `(a)` → **`CallExpr(GetAttrExpr(...), [a])`**
- `.b` → **`GetAttrExpr(CallExpr(...), "b")`**

**Anidados** = cada paso envuelve el resultado anterior: el árbol crece hacia afuera como capas de cebolla.

- **`GetAttrExpr`**: expresión que **accede a un atributo/método** de un objeto (`obj.attr`).
- **`CallExpr`**: expresión que **llama** a algo invocable (función global o resultado de `GetAttr`) con argumentos.

**`LetExpr` anidados**

`let x = 1, y = 2 in expr` en el CST es una lista de bindings + cuerpo. En el AST se convierte en **varios `LetExpr` anidados**, de adentro hacia afuera:

```
LetExpr(x, 1,
  LetExpr(y, 2,
    expr))
```

Se itera **de derecha a izquierda** sobre los bindings (`build_let_expr`, bucle `for (i = size-1; i >= 0; --i)`), envolviendo el cuerpo en cada `LetExpr`. Cada nodo = un binding + el sub-AST que queda “debajo”.

**`IfExpr` anidados (elif)**

`if c1 then t1 elif c2 then t2 else e` no tiene nodo `ElifExpr`. Se normaliza a **`IfExpr` encadenados**:

```
IfExpr(c1, t1,
  IfExpr(c2, t2, e))
```

Cada `elif` es el **`else`** del `if` anterior (`build_elif_chain_opt`).

**Representación OO unificada**

- **`obj.m(a)`** → `CallExpr(callee = GetAttrExpr(obj, "m"), args = [a])`
- **`obj.attr := v`** → `AssignExpr(lhs = GetAttrExpr(obj, "attr"), op = :=, rhs = v)`

**¿Por qué simplifica?**

En lugar de 4 tipos distintos (`MethodCallExpr`, `FunctionCallExpr`, `SetAttrExpr`, `VarAssignExpr`), el semántico y el codegen tienen **2 visitantes genéricos**:

- **`CallExpr`**: si `callee` es identificador → función global; si es `GetAttrExpr` → método (véase `visit(CallExpr*)` en `SemanticCheck/phase2_checker.cpp`).
- **`AssignExpr`**: si `lhs` es identificador → variable; si es `GetAttrExpr` → atributo de objeto.

Menos casos especiales, misma lógica de tipos/argumentos para llamadas y de conformidad para asignaciones.

---

## 5.1 — Estructura de dos pasadas

en esta parte 

5. Análisis semántico
5.1 Estructura de dos pasadas
El análisis semántico se divide en una pasada de recolección de declaraciones y otra de verificación de tipos. Esta separación permite que las declaraciones orientadas a objetos referencien tipos registrados pero aún no analizados completamente dentro de la misma unidad de compilación.

a que le llamas una pasada de recoleccion de declaraciones y otra de verificacion de tipos y por que Esta separación permite que las declaraciones orientadas a objetos referencien tipos registrados pero aún no analizados completamente dentro de la misma unidad de compilación. explicame esto con mas detalles

### Respuesta — dos pasadas

**Pasada 1: recolección de declaraciones**

Recorre el AST de **nivel superior** y **registra nombres y firmas** en la `SymbolTable` **sin analizar a fondo** los cuerpos (o con tipos `Unknown`).

**¿Para qué?** Para que, cuando más adelante analices un cuerpo, **ya existan** los símbolos referenciados.

**Ejemplo OO:** en el archivo puedes tener:

```hulk
let p = new Point(1, 2) in print(p.x);
type Point(x, y) { ... }
```

El `new Point` aparece **antes** del `type Point` en el texto. La pasada de recolección registra `Point` primero; luego la verificación puede validar `new Point(...)` aunque la clase aún no se haya analizado completamente.

En el proyecto: `collectClassDeclarations` + `collectFunctionDeclarations` al inicio de `Phase2Analyzer::analyze`, y también `SymbolTable/decl_collector.cpp` para el modo `--symbols`.

**Pasada 2: verificación de tipos**

Recorre cuerpos, expresiones, comprueba tipos, herencia, llamadas, etc., usando la tabla ya poblada.

---

## Pasada de recolección (decl_collector)

La pasada de recolección (decl_collector.cpp) recorre las sentencias de nivel superior y registra firmas de funciones (con sobrecarga por aridad), nombres de clases, aristas de herencia, nombres de atributos y firmas de métodos. Las funciones predefinidas y las constantes globales (PI, E) se inyectan antes del código del usuario.

en esta otra parte explicame que son las sentencias de nivel superior, que significa que las recorra y regsitra firmas de funciones, que son firmas de fucniones, y que es con sobre carga de paridad, que son aristas de herencia, nombres de clases, nombres de atributos y firmas de metodos, para qué se hace esto? CUales son las funciones predefinidas y cuales son las constantes globales

### Respuesta — vocabulario de recolección

| Término | Significado |
|--------|-------------|
| **Sentencias de nivel superior** | Hijos directos de `Program`: `function ...`, `type ...`, y expresiones globales (`print(...);`, `let ... in ...`). No cosas dentro de un `let`, método, etc. |
| **Recorrer y registrar firmas** | Guardar en la tabla que existe `f` con N parámetros y tipo de retorno (aún puede ser `Unknown`). |
| **Firma de función/método** | Nombre + lista de parámetros (nombres y tipos) + tipo de retorno. Ej: `greet(name: String): String`. |
| **Sobrecarga por aridad** | Mismo nombre, **distinto número de argumentos**. Válido: `f(a) => ...` y `f(a,b) => ...`. Inválido: dos `f(a) => ...`. |
| **Aristas de herencia** | Relaciones padre→hijo: `Circle inherits Shape` registra que `Circle` hereda de `Shape`. |
| **Nombres de clases/atributos/métodos** | Metadatos OO en la tabla de tipos para resolver `.attr`, `.method()`, `new`, etc. |

**Funciones predefinidas (builtins):** `sin`, `cos`, `sqrt`, `exp`, `log`, `rand`, `print` (0 o 1 arg), `range`.

**Constantes globales:** `PI`, `E` (tipo `Number`, no redefinibles).

Se registran en el constructor de `SymbolTable` (`addBuiltinVariables`, `addBuiltinFunctions` en `SymbolTable/symbol_table.hpp`).

---

## R2 — Definida antes de uso

**R2 — Definida antes de uso.** Toda variable debe estar definida antes de aparecer en una expresión. En `let a = 1, b = a + 1 in …`, los enlaces se procesan de izquierda a derecha de modo que `b` puede referenciar `a`. La autorreferencia en un inicializador (`let x = x in …`) se rechaza. Las funciones son solo globales y no capturan variables de ámbitos `let` exteriores.

let a = 1, b = a + 1 in print(b);

let a = 1, b = a + 1 in let a = 2 in print(b);


DE AQUI QUE SIGNIFICA BIEN QUE LAS FUNCIONES SON SOLO GLOBALES Y NO CAPTURAN VARIABLES DE AMBITOS LET EXTERIORES, 

### Respuesta — R2

Significa dos cosas relacionadas:

1. **`function` solo como sentencia global** — no puedes escribir `let x = 1 in function f() => x + 1 in ...` (error **sintáctico**: `function` no es expresión).
2. **Las funciones no ven variables de un `let` de otra sentencia** — al analizar el cuerpo de una función, el ámbito tiene **solo parámetros** (y globals/builtins). Si dentro de la función usas una variable que solo existe en un `let` de otra línea, obtienes **`UNDEFINED_VARIABLE`**.

No hay **closures** (funciones que “capturan” variables del entorno léxico). Para usar un valor exterior, **pásalo como argumento**.

Tus ejemplos de `let`:

```hulk
let a = 1, b = a + 1 in print(b);           // b = 2 (a es 1 al definir b)
let a = 1, b = a + 1 in let a = 2 in print(b);  // b sigue siendo 2; el a=2 interior no afecta b
```

---

## R3 — Parámetros únicos y sobrecarga

**R3 — Parámetros únicos y sobrecarga.** Los nombres de parámetros deben ser únicos dentro de la firma de una función o método. Varias funciones globales pueden compartir nombre si difieren en aridad; redefinir el mismo nombre con la misma aridad es un error.

PARA ESTE CASO QUE SIGNIFICA QUE LO NOMBRES DE PARAMETROS DEBEN SER UNICOS DENTRO DE LA FIRMA DE UNA FUCNION O METODO, QUE ES LA FIRMA DE UNA FUNCION O METODO?? qUE SIGNIFICA QUE VARIAS FUNCIONES GLOBALES PUEDEN COMPARTIR NOMBRE SI DIFIEREN EN ARIDAD

COMO PODRIA VERIFICAR QUE REDEFINIR EL MISMO NOMBRE CON LA MISMA ARIDAD ES UN ERROR

### Respuesta — R3

- **Parámetros únicos en la firma:** en `function f(x, y, x) => ...` no puedes repetir `x` en la **misma** lista de parámetros. Se verifica con `checkUniqueParamNames` (set de nombres vistos).
- **Firma:** nombre + parámetros + retorno de **una** función concreta.
- **Mismo nombre, distinta aridad:** sobrecarga válida; la tabla guarda **varias entradas** bajo el mismo nombre, distinguidas por `params.size()`.
- **Misma aridad → error:** `declareFunction` rechaza si ya existe una overload con el mismo número de parámetros → `REDEFINED_FUNCTION`.

**Cómo verificarlo:** archivo de prueba `tests/semantic/invalid/13_r3_overload_same_arity.hulk`:

```hulk
function suma(a, b) => a + b;
function suma(x, y) => x + y;   // misma aridad → error
```

Ejecutar: `./hulk tests/semantic/invalid/13_r3_overload_same_arity.hulk --semantic` → debe reportar redefinición.

---

## 5.3 — Semántica orientada a objetos

### 5.3 Semántica orientada a objetos

Para cada declaración `type`, el analizador valida que los objetivos de `inherits` existen, detecta ciclos de herencia, registra atributos en el ámbito de la clase y analiza cuerpos de métodos en un ámbito donde `self` tiene el tipo de la clase actual. Las comprobaciones de compatibilidad de sobrescritura de métodos comparan tipos de retorno y listas de parámetros con los métodos del padre a lo largo de la cadena de herencia.

La expresión `new` se valida contra la lista de parámetros del constructor del tipo objetivo. Las expresiones de inicialización de atributos se verifican en el contexto de la clase. Las llamadas a `base()` están permitidas solo dentro de métodos de clases con padre.

 explicame aqui con mas detalles que significa que el analizador valida que los objetivos de 'inherits' existen, que detecta ciclos de herencia, que registra atributos en el ambito de la clase y que analiza cuerpos de metodos en un ambito donde self tiene el tipo de la clase actual, que significa que self tiene el tipo de la clase actual.
 
 que significa que las comprobaciones de compatibilidad de sobreescritura de metodos comparan tipos de retorno y lista de parametros con los metodos del padre a lo largo de la cadena de herencia, 
 
 de todos estos procesos tambien explicame como es que se hacen en el proyecto

La expresión `new` se valida contra la lista de parámetros del constructor del tipo objetivo. Las expresiones de inicialización de atributos se verifican en el contexto de la clase. Las llamadas a `base()` están permitidas solo dentro de métodos de clases con padre.
 de aqui dime como es que la expresion new se valida contra la lista de parámetros del constructor del tipo objetivo
 como las expresiones de inicializacion de atributos se verifican en el contexto de la clase.
 que es que las llamadas a base() están permitidas solo dentro de métodos de clases con padre
 
 dime como se usa la expresion new y la expresion base en el lenguaje?

### Respuesta — semántica OO

| Proceso | Significado | Dónde en el proyecto |
|--------|-------------|----------------------|
| Validar `inherits` existe | El padre nombrado está declarado (builtin o `type` del usuario) | `registerClassDecl`: comprueba `isTypeDeclared(base)` |
| Detectar ciclos | `A inherits B inherits A` o `A inherits A` | `validateInheritanceChain`: recorre padres con un `set` de visitados |
| Registrar atributos en ámbito de clase | Los attrs quedan en el `TypeSymbol` para `lookupAttribute` | `addTypeAttribute` en `registerClassDecl` |
| Analizar métodos con `self` tipado | Al revisar cuerpos, `self` se declara como `Object(NombreClaseActual)` | `visit(ClassDecl*)`: `declareVariable("self", Object(class_name))`; `visit(SelfExpr*)` usa `current_class_` |
| **`self` tiene el tipo de la clase actual** | Dentro de `type Knight ...`, `self` es `Knight`, no `Object` genérico ni el padre |
| Compatibilidad de sobrescritura | Si hijo redefine método del padre, **misma aridad** y tipos de parámetros compatibles; retorno del hijo debe **conformar** al del padre | `methodSignatureMatchesOverride` + `validateMethodOverrideReturns` |

**`new` — validación**

**Uso en el lenguaje:** `new NombreTipo(arg1, arg2, ...)` crea una instancia.

**Validación** (`visit(NewExpr*)` en `phase2_checker.cpp`): busca la declaración del tipo, compara **cantidad** de args con `class_decl->params`, y cada arg con el tipo del parámetro del constructor vía `conformsTo`.

**Inicialización de atributos**

En `visit(ClassDecl*)`, con `self` y params ya en ámbito, se evalúa cada `attr = expr` y se comprueba que el tipo de `expr` conforma al tipo declarado (o se infiere si no hay anotación).

**`base()`**

**Qué es:** llamada al **constructor/método del padre** dentro de un método de una subclase.

**Restricción:** solo válido en métodos de clases que **tienen padre distinto de `Object`**. Si no, `INVALID_BASE` en `visit(BaseCallExpr*)`.

**Ejemplo** (de `playground/test15.hulk`):

```hulk
type Dog(name) {
  speak() => "Guau";
}
type GoldenRetriever(name) inherits Dog {
  speak() => base() @ " (ladra: Woof!)";
}
let g = new GoldenRetriever("Buddy") in print(g.speak());
```

`base()` en `speak` delega al comportamiento del padre (`Dog`).

---

## 5.4 — Inferencia de tipos

 ### 5.4 Inferencia de tipos

HULK permite omitir anotaciones de tipo en varias posiciones: variables locales en `let`, tipos de retorno de funciones y métodos, y tipos de atributos. El analizador ejecuta hasta diez pasadas de punto fijo sobre cuerpos de funciones y métodos hasta que los tipos se estabilizan o se reporta un error. Los operadores binarios numéricos, de cadena y booleanos tienen reglas de propagación que asignan tipos a referencias de identificadores sin tipo cuando se conoce el tipo del otro operando.

en esta parte explicame por que El analizador ejecuta hasta diez pasadas de punto fijo sobre cuerpos de funciones y métodos hasta que los tipos se estabilizan o se reporta un error.

y cuando se reporta un error, y como funcionan esas pasadas que se hace en ellas, por que son diez y no son menos o mas

### Respuesta — inferencia de tipos

Algunos tipos **dependen de otros** que a su vez dependen del primero:

```hulk
function f(x) => g(x);
function g(y: Number) => y + 1;
```

En la pasada 1, `x` puede ser `Unknown`; en la 2, `g` fija que el arg es `Number`; en la 3, `x` se propaga a `Number`. Hasta que **nada cambia** → **punto fijo**.

**Qué hace cada pasada** (`runInferencePasses` en `phase2_checker.cpp`): re-analiza funciones, métodos y sentencias; si algún tipo en la firma cambió (`analyzeFunctionDecl` devuelve `changed = true`), hay otra pasada.

**¿Por qué 10?** Es un **límite de seguridad** contra ciclos raros o no convergencia. En la práctica bastan pocas; 10 es generoso. Si tras 10 pasadas aún cambia, simplemente se detiene (no hay error automático por “no convergió en 10”; el error aparece si los tipos quedan inconsistentes en verificaciones concretas).

**Cuándo se reporta error:** cuando una regla concreta falla (tipos incompatibles, variable indefinida, etc.), no necesariamente “por agotar las 10 pasadas”.

---

## 5.5 — Sistema de tipos

### 5.5 Sistema de tipos

`Types/type_info.cpp` modela `Number`, `String`, `Boolean`, `Null`, tipos objeto y tipos desconocidos o inferidos. El subtipado a lo largo de aristas de herencia se verifica mediante `conformsTo`. El operador `is` produce un `Boolean`; `as` produce el tipo objetivo cuando el cast es estáticamente posible, con comprobaciones en tiempo de ejecución emitidas por el generador de código para los casos dinámicos.

en esta parte explicame que significa `Types/type_info.cpp` modela `Number`, `String`, `Boolean`, `Null`, tipos objeto y tipos desconocidos o inferidos.

que significa que El subtipado a lo largo de aristas de herencia se verifica mediante `conformsTo`. y como se hace este proceso


explicame por que El operador `is` produce un `Boolean`; `as` produce el tipo objetivo cuando el cast es estáticamente posible, con comprobaciones en tiempo de ejecución emitidas por el generador de código para los casos dinámicos.

que significa que `as` produce el tipo objetivo cuando el cast es estáticamente posible, con comprobaciones en tiempo de ejecución emitidas por el generador de código para los casos dinámicos.

### Respuesta — sistema de tipos

**Qué modela `type_info.cpp`**

`TypeInfo` representa tipos HULK con un `Kind`:

- **`Number`, `String`, `Boolean`, `Null`** — primitivos.
- **`Object`** — tipos nominativos (`Point`, `Shape`) con nombre.
- **`Unknown`** — aún no inferido/anotado.
- También **`Void`**, **`Function`**, etc. para usos internos.

**`conformsTo` y subtipado**

**`T1.conformsTo(T2)`** ≈ “¿puedo usar un valor de tipo T1 donde se espera T2?”

Reglas principales (en `Types/type_info.cpp`):

- Igualdad exacta.
- Todo conforma a **`Object`**.
- **`Null`** conforma a tipos objeto.
- Si ambos son objetos nominales: **`isSubtypeOf`** recorre la cadena `Circle → Shape → Object` subiendo por `parent_name` en la tabla.

**`is` y `as`**

- **`is`**: expresión de tipo **`Boolean`** — “¿este valor es de tipo T?”  
  En codegen, para objetos compara el **tipo runtime** de la instancia (`emitRuntimeTypeConforms` en `Codegen/llvm_codegen.cpp`).

- **`as`**: expresión del **tipo objetivo** del cast.  
  - **Estáticamente posible:** el compilador acepta el cast (p. ej. subtipo conocido en compile-time).  
  - **Dinámico:** si en compile-time no se puede garantizar (variable polimórfica), el **generador emite comprobación en runtime**; si falla, llama a `hulk_runtime_cast_error` (error en **ejecución**, no diagnóstico de compilación).

---

## 5.6 — Reporte de errores

### 5.6 Reporte de errores

Los errores semánticos se recogen en un `ErrorManager` y se reportan como `(línea,col) SEMANTIC: mensaje` en la salida de error estándar con código de salida 3. Los diagnósticos idénticos duplicados se colapsan antes de la salida. El modo desarrollo puede listar todos los errores semánticos en una sola ejecución; el pipeline de producción reporta errores solo de la primera fase que falla (los léxicos tienen prioridad sobre los sintácticos, y estos sobre los semánticos).

en esta parte explicame con mas detalles que significa que los diagn'osticos identicos duplicados se colapsan antes de la salida, que son los diagnosticos identicos duplicados.
explicame si que el pipeline de produccion reporta errores solo de la primera fase que falla esta correcto (los léxicos tienen prioridad sobre los sintácticos, y estos sobre los semánticos= esta correcto? creo que la orden del proyecto pide que se muestren todos los errores de todas las fases, corrobora esto y dime si es posible que el funcionamiento sea así

### Respuesta — reporte de errores

**Diagnósticos idénticos duplicados**

Dos (o más) entradas con **exactamente el mismo texto formateado**, por ejemplo:

```
(1,7) SEMANTIC: Variable 'z' no está definida
(1,7) SEMANTIC: Variable 'z' no está definida
```

Pueden surgir si **dos rutas del analizador** reportan el mismo problema. En `Compiler/pipeline.cpp` → `fail_semantic`, antes de imprimir se **colapsan** (solo se guarda la primera ocurrencia de cada línea idéntica).

**¿Es correcto “solo la primera fase que falla”?**


> Prioridad: léxico > sintáctico > semántico (**solo se reporta la primera fase que falla**).

Implementación en `Compiler/pipeline.cpp`:

1. Si hay token `UNKNOWN` → **exit 1**, no parsea ni analiza semántica.
2. Si el parser lanza `ParseError` → **exit 2**, no hay semántica.
3. Si la semántica falla → **exit 3**, y ahí sí puede haber **varios** errores semánticos (todos los que acumule `Phase2Analyzer`, menos duplicados exactos).

**No** es requisito del proyecto matcom mostrar errores de **todas las fases a la vez**. De hecho, **no tendría sentido** en general: con errores léxicos graves no obtienes AST fiable; con errores sintácticos no tiene sentido correr semántica completa.

**Modo desarrollo** (`--semantic`): imprime **todos los errores semánticos** (`analyzer.printErrors()`), pero **igual** para antes si el parse falla. Tampoco mezcla fases.

Si en algún enunciado del curso leíste “mostrar todos los errores”, suele referirse a **todos los errores de la fase semántica**, no a mezclar léxico + sintáctico + semántico en una sola ejecución.
