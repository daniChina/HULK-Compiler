respondeme aqui por que no los da, y como se podria solucionarlo
Y qué otros casos podrían fallar de la misma manera que estos, por que fallarian y como los solucionarias
SOLO RESPONDE LAS PREGUNTAS, NO LAS RESUELVAS

**Respuesta:** Los tokens del lexer sí llevan `line` y `col` correctos desde el origen. El problema aparece al **reportar** el error en contextos anidados:

1. **Errores semánticos de tipo** suelen ubicarse en el **operador** (`+`, `==`, etc.) y no en el operando concreto que provoca el conflicto. Ejemplo: en `print(1 + "bad")` el compilador puede reportar la columna del `+` (posición 13 en la línea del bloque) en lugar de la cadena `"bad"`.
2. **Errores dentro de llamadas anidadas** (`print(foo(1 + bar))`) reportan la posición del identificador interno, pero el analizador de inferencia multipasada puede **duplicar** el mismo diagnóstico 2–3 veces (una por pasada de punto fijo).
3. **Errores dentro de métodos** siguen el mismo patrón: se usa el token del nodo AST visitado (identificador, operador), no siempre el token más específico del usuario.
4. **Expresiones en `{…}`** no alteran el conteo de líneas del lexer; la fila debería ser la del código real. Si la columna falla, suele ser porque el nodo que reporta el error no es la hoja terminal del problema.

**Cómo se podría solucionar (sin implementarlo aquí):**
- Reportar siempre la posición del **token terminal más específico** (identificador, literal) en errores de tipo, no la del operador padre.
- Colapsar diagnósticos idénticos antes de imprimir (el `ErrorManager` ya lo hace en producción, pero `--semantic` en desarrollo puede listar duplicados).
- Propagar `span` de error desde el hijo al padre cuando el padre solo agrega contexto.

**Otros casos que podrían fallar igual y por qué:**

| Caso | Por qué fallaría la posición | Solución conceptual |
|------|------------------------------|---------------------|
| Error en argumento 3 de una llamada con muchos args | El checker reporta el `CallExpr` padre | Usar `col` del argumento concreto en el vector de args |
| Error en cuerpo de `function f() { … }` extendida | El bloque no es un token; se usa la primera expr del bloque | Guardar posición de cada stmt del bloque en el AST |
| Error en `obj.m(1 + "x")` | Mismo patrón que llamada anidada | Reportar en el operando del `BinaryExpr` interno |
| Error en `if (x > "a") …` | Condición reportada en `>` no en `"a"` | Igual que aritmética: span del operando incompatible |
| Error en inicializador de atributo de clase | Atributo declarado en otra línea que la expr | Usar token del init, no del nombre del atributo |

**Casos de prueba sugeridos:**

```hulk
// TEST pos-01 — error de tipo dentro de llamada anidada
// Esperado: error semántico TYPE_ERROR; columna debe apuntar cerca de "bad" (ideal) o al '+' (actual)
print(foo(1 + "bad"));
function foo(x) => x;

// TEST pos-02 — variable indefinida dentro de argumento anidado
// Esperado: UNDEFINED_VARIABLE en 'zzz'; columna ~26 en línea de inner(); sin duplicar el mensaje
function outer() => inner(1);
function inner(x) => x + zzz;

// TEST pos-03 — error de tipo dentro de bloque let
// Esperado: TYPE_ERROR línea 2; columna ideal ~18 ("bad"), puede dar ~13 ('+')
let x = 1 in {
    print(1 + "bad");
};

// TEST pos-04 — error dentro de método
// Esperado: UNDEFINED_VARIABLE en 'bad', línea del cuerpo del método
type P() { m() => print(bad); }
new P().m();

// TEST pos-05 — error tras comentario de bloque (posición absoluta)
// Esperado: TYPE_ERROR línea 4 (no línea 1); columna en el operando problemático
/*
comentario largo
*/
print(1 + "x");

// TEST pos-06 — cadena de funciones con error en la más interna
// Esperado: UNDEFINED_VARIABLE en 'z' en la columna exacta de 'z'
function a(x) => b(x);
function b(x) => c(x);
function c(x) => x + z;
a(1);
```





hay errores interesantes meter errores dentro de llamados a funciones,
dentro de expresiones en {}

dan mal los valores de columnas y fila del error,
quizas dentro de metodos, y dentro de otros tipos de expresiones que encapsulen suceda de manera similar, analiza esto y dime que otros casos de pruebas se podrían generar para comprobar que esto suceda bien y como solucionarlo

NO RESUELVAS EL PROBLEMA, RESPONDEME LA PREGUNTA

**Respuesta:** Misma causa raíz que arriba. Además conviene probar:

- Errores **sintácticos** dentro de `{…}` (falta `;`, `let` sin `in`) → el parser usa el token encontrado, suele ser más preciso.
- Errores **semánticos** en expresiones que son **última expr de un bloque** vs expr intermedia.
- Errores en **indexado** `v[i + "x"]`, **concatenación** `"a" @ (1 + "b")`, **cast** `x as Unknown`.

**Casos adicionales:**

```hulk
// TEST pos-07 — error sintáctico dentro de bloque (parser)
// Esperado: SYNTACTIC, token inesperado; fila/columna del token real
{ let x = 1; let x = 2; x };

// TEST pos-08 — error en indexado
// Esperado: TYPE_ERROR en el índice no-Number
let v = [1,2,3] in print(v["a"]);

// TEST pos-09 — error en concatenación anidada
// Esperado: TYPE_ERROR en operando inválido de @
print("a" @ (true @ false));
```


BUSCAR LA EXTENSION QUE MANDO PIAD PARA COMPROBAR EL LENGUAJE Y VER QUE CASOS SE PUDIERAN CREAR A PARTIR DE LOS ERRORES QUE ELLA DESCUBRE PARA CREAR NUEVOS ERRORES Y COMPROBAR EL FUNCIONAMIENTO DEL COMPILADOR

**Respuesta:** En este repositorio **no está incluida** la extensión que menciona Piad (probablemente una extensión de VS Code / Cursor para resaltar, ejecutar o validar HULK). Hay que buscarla en el material del curso (correo, Moodle, repo del profesor) o preguntar el identificador exacto en el marketplace (`hulk`, `matcom-hulk`, etc.).

**Qué hacer con ella:** usarla como **orquestador manual** — abrir fixtures `.hulk`, ejecutar el compilador integrado y anotar errores que la extensión muestre distinto al CLI. Cada discrepancia (posición, tipo de error, mensaje) es candidato a un fixture nuevo.

**Casos derivados de errores típicos que una extensión suele descubrir:**
- Resaltado incorrecto de string sin cerrar → fixture léxico.
- Autocompletado que sugiere keyword inválida en contexto → fixture sintáctico.
- Subrayado de variable “no definida” en shadowing válido → fixture semántico R1.
- Falso positivo en `function` usada antes de declarar → fixture R4.


VER TAMBIEN QUE TIPOS NUMERICOS SE ACEPTAN EN EL LENGUAJE, SEGUN LO QUE ESTA IMPLEMENTADO Y LO QUE ESTA EN LA DEFINICION DEL LENGUAJE, Y VER QUE PROBLEMAS PODRIA DAR EL TRABAJO CON ESOS TIPOS NUMERICOS Y COMO LOS ENFRENTA EL COMPILADOR PARA VER SI LOS ENFRENTA CORRECTAMENTE

**Respuesta según implementación actual (`Lexer/hulk_lexer.l` + semántica):**

| Forma | ¿Aceptada? | Notas |
|-------|------------|-------|
| Enteros `42`, `0` | Sí | Token `NUMBER_LITERAL`, tipo `Number` |
| Flotantes `3.14`, `0.5` | Sí | Regla `FLOAT` antes que `INT` |
| Notación científica `1e10` | **No** en lexer actual | No hay regla para `e`/`E` en numerales |
| Hex/oct/bin | **No** | Solo decimal |
| `PI`, `E` | Sí | Variables globales builtin, tipo `Number`, inmutables |
| Negativos `-5` | Sí | Operador unario `-` sobre literal |

**Problemas potenciales:**
- Todo numérico es `Number` (double); no hay `Int` separado → `%` y comparaciones funcionan, pero precisión flotante en tests de igualdad estricta.
- Mezclar `Number` con `String` en `+` → `TYPE_ERROR` (no concatena con `+`, solo con `@`).
- Inferencia: `let x = 1` infiere `Number`; anotar `Number` explícito debe coincidir.

**Casos de prueba:**

```hulk
// TEST num-01 — entero y flotante
// Esperado: imprime 42 y 3.14
print(42); print(3.14);

// TEST num-02 — aritmética mixta entero/flotante
// Esperado: 2.5
print(5 / 2);

// TEST num-03 — PI y E builtin
// Esperado: ~3.14159..., ~2.71828...; no deben ser redeclarables
print(PI); print(E);

// TEST num-04 — suma Number + String (inválido)
// Esperado: TYPE_ERROR
let x = 1 + "2" in print(x);

// TEST num-05 — concatenación Number a String con @ (válido)
// Esperado: "valor: 42"
print("valor: " @ 42);

// TEST num-06 — anotación explícita vs inferida
// Esperado: compila; imprime 7
let x:Number = 3, y = 4 in print(x + y);
```


OTRO CASO QUE PUEDE PARTIRSE Y QUE SERIA BUENO COMPROBAR COMO FUCNIONE ES EL DE ENCADENAR FUCNIONES, PONER FUNCIONES DENTRO DE FUNCIONES Y PONER ERRORES DENTRO DE LA FUCNION MAS ADENTRO A VER SI MUESTRA BIEN LA FILA Y LA COLUMNA

**Respuesta:** El error debe reportarse en la **función más interna donde ocurre el identificador/tipo inválido**, no en la llamada externa. Riesgo: si el checker propaga error al padre, la fila sigue siendo correcta pero la columna puede ser la del call exterior. Ver también TEST pos-06 arriba.

**Casos de prueba:**

```hulk
// TEST chain-01 — error semántico en el eslabón más profundo
// Esperado: UNDEFINED_VARIABLE 'innermost' en línea de c(), columna de 'innermost'
function a() => b();
function b() => c();
function c() => innermost;
a();
```

QUE SUCEDERIA EN EL CASO DE PONER UN PRINT, DENTRO DE UN  PRINT DENTRO DE OTRO PRINT, DE ACUERDO CON LAS REGLAS DEL LENGUAJE ESO ESTARÍA CORRECTO, QUÉ SUCEDE EN C# EN UN CASO COMO ESE??

**Respuesta HULK:** `print` es una función builtin cuyo valor de retorno es **`Null`** (no `void` sintáctico). Por tanto **`print(print(1))` es sintáctica y semánticamente válida**: el `print` interno imprime `1` y devuelve `Null`; el externo imprime ese `Null` (según formato de impresión del runtime).

**Respuesta C#:** `Console.WriteLine(Console.WriteLine(1))` **no compila**, porque `WriteLine` devuelve `void` y `void` no es un argumento válido. En C# lo idiomático es `{ Console.WriteLine(1); Console.WriteLine(2); }`.

**Casos de prueba:**

```hulk
// TEST print-nest-01 — doble print (válido en HULK)
// Esperado: imprime 1, luego imprime null/Null (según runtime)
print(print(1));

// TEST print-nest-02 — triple anidamiento
// Esperado: tres líneas de salida; compila sin error
print(print(print(2)));

// TEST print-nest-03 — print en expresión aritmética (inválido)
// Esperado: TYPE_ERROR (Null/Unknown no combina con Number en +)
print(1 + print(2));
```


ME FALTA COMPROBAR EL FUNCIONAMIENTO DE LAS EXPRESIONES BOOLEANAS A VER SI FUNCIONA BIEN, Y EL FUNCIONAMIENTO DE MEZCLAR BOOLEANAS CON OTROS TIPOS CON NUMBER, CON STRING

**Respuesta:** Operadores booleanos (`&`, `|`, `!`) exigen operandos `Boolean`. Condiciones de `if`, `while`, `unless`, `repeat` también. Comparaciones (`==`, `<`, etc.) tienen reglas por par de tipos: numérico-numérico, string-string, boolean-boolean; mezclas inválidas → `TYPE_ERROR`.

**Casos de prueba:**

```hulk
// TEST bool-01 — operadores booleanos puros (válido)
// Esperado: "no and", "or", "not false"
print(if (true & false) "and" else "no and");
print(if (true | false) "or" else "no or");
print(if (!false) "not false" else "false");

// TEST bool-02 — if con condición numérica (inválido)
// Esperado: TYPE_ERROR
if (1) print(1) else print(2);

// TEST bool-03 — mezcla Boolean + Number en + (inválido)
// Esperado: TYPE_ERROR
let x = true + 1 in print(x);

// TEST bool-04 — comparación válida numérica
// Esperado: imprime "ok"
if (1 + 2 == 3) print("ok") else print("fail");

// TEST bool-05 — comparación String == Number (inválido)
// Esperado: TYPE_ERROR
print("a" == 1 + 2);

// TEST bool-06 — ! sobre Number (inválido)
// Esperado: TYPE_ERROR
print(!1);
```

TAMBIEN ME FALTA MEZCCLAR OPERACIONES ENTRE STRING, BOOLEANAS, Y NUMBER Y VER SU FUNCIONAMIENTO EN CASO DE QUE SEAN EXPRESIONES QUE NO SE DEBAN PROCESAR, MEZCLAR OPERACIONES MAL MEZCLADAS PERO QUE SEAN ENCADENADAS EJEMPLO SUMA UNA COMPARACION DE STRING O COMPARA UNA SUMA DE STRING, O SUMA UNA CONCATENACION DE BOOLEANOS

**Respuesta:** El checker evalúa **prioridad de operadores** primero: `1 + "a" == 2` parsea como `(1 + "a") == 2` → falla en el `+` antes de llegar a la comparación. `"a" == 1 + 2` falla en la comparación String/Number. `true @ false` falla porque `@` exige String en al menos un lado (reglas de concat).

**Casos de prueba:**

```hulk
// TEST mix-01 — suma luego comparación (inválido en la suma)
// Esperado: TYPE_ERROR en '+'
let x = (1 + "a") == 2 in print(x);

// TEST mix-02 — comparación de suma de strings (válido si ambos string)
// Esperado: imprime true o false según valores
print(("a" @ "b") == "ab");

// TEST mix-03 — suma de concatenación booleana (inválido)
// Esperado: TYPE_ERROR en @ o en +
print(true @ false + 1);

// TEST mix-04 — comparar suma numérica con string (inválido en ==)
// Esperado: TYPE_ERROR
print((1 + 2) == "3");
```


OTRO CASO DE PRUEBA QUE VALE LA PENA COMPROAR ES EN EL QUE ESCRIBO UN COMENTARIO EN EL INTERMEDIO DE UN CODIGO Y VER SI AL CODIGO QUE SIGUE, SI LE SALE ALGUN ERROR MUESTRA BIEN LA POSICION DE COLUMNA Y FILA

EJEMPLO

CODIGO

/*
COMENTARIO
*/

CODIGO CON ERROR



O 
CODIGO

//COMENTARIO DE LINEA

//COMENTARIO DE LINEA

CODIGO CON ERROR

**Respuesta:** Los comentarios `//` y `/* */` se descartan en el lexer **sin emitir tokens**, pero **sí avanzan** `line_`/`column_`. El código posterior conserva la numeración correcta de filas. Tras comentarios largos, la columna del error debe ser relativa a la línea del código con error, no al comentario.

**Casos de prueba:**

```hulk
// TEST cmt-01 — bloque /* */ antes del error
/*
COMENTARIO
*/
print(1 + "x");
// Esperado: TYPE_ERROR en línea 5 (4 si no hay línea en blanco final), columna del '+' o "x"

// TEST cmt-02 — comentarios de línea intercalados
print(1);
// COMENTARIO DE LINEA
// OTRO COMENTARIO
print(2 + "x");
// Esperado: TYPE_ERROR en línea 5, no en línea 1

// TEST cmt-03 — comentario en la misma línea del error
print(1 + "x"); // error aquí
// Esperado: TYPE_ERROR línea 1; verificar que columna no cuenta el comentario como código
```



REVISAR CUALES SON LOS CARACTERES QUE NO SON VALIDOS EN EL LENGUAJE Y QUE SUCEDE SI LOS PONGO DENTRO DE UN STRING O DENTRO DE UN COMENTARIO PUES CREO QUE EN ESTOS CASOS LOS DEBERIA PERMITIR, REVISAR SI LOS PERMITE

**Respuesta según lexer:**
- **Fuera de strings/comentarios:** cualquier carácter no reconocido → token `UNKNOWN` → error léxico `(l,c) LEXICAL`.
- **Dentro de strings `"…"`:** casi cualquier carácter es válido; `\` inicia escapes (`\n`, `\t`, `\"`, `\\`); otro escape → se conserva literalmente.
- **Dentro de `//…`:** todo hasta fin de línea es ignorado → **`$`, `@`, unicode, etc. permitidos**.
- **Dentro de `/* … */`:** igual → **permitidos**.
- **`$` no es operador HULK** en el lexer actual; fuera de string/comentario sería inválido.

**Casos de prueba:**

```hulk
// TEST char-01 — carácter inválido suelto (léxico)
// Esperado: LEXICAL unexpected character '$'
print($);

// TEST char-02 — $ dentro de string (válido)
// Esperado: imprime "$ok"
print("$ok");

// TEST char-03 — $ solo en comentario (válido)
// $ esto no es código
print(1);

// TEST char-04 — @ fuera de string en código inválido como identificador
// Esperado: error sintáctico o léxico
let x = @ in print(x);
```



Restricción de bloques. La gramática define que FIRST(Expr) no incluye LBRACE, por lo que los bloques {…} aparecen solo en posiciones explícitamente permitidas (cuerpos de funciones, de let/if/while, etc.) y no pueden usarse como operandos arbitrarios. 
HACER CASO DE PRUEBA QUE COMPRUEBEN ESTO, EN QUE EXPRESIONES SON VALIDAS EL USO DE BLOQUES Y EN CUALES NO

**Respuesta:** `FIRST(Expr)` no incluye `LBRACE`. Un bloque **no es** una expresión primaria; solo aparece donde la gramática lo permite explícitamente:

**Válido:** cuerpo de `let … in {…}`, `if/while/unless/repeat` con `{…}`, cuerpo extendido de `function`, cuerpo de `case`, `with`, sentencia de nivel superior `{…}`, cuerpo de método.

**Inválido:** operando de `+`, `-`, `*`, argumento de llamada (salvo que la llamada esté dentro del bloque), rhs de `in` sin paréntesis, `{…} + {…}`, `let x = {0;1} in …`.

**Casos de prueba:**

```hulk
// TEST blk-01 — bloque como operando (inválido)
// Esperado: SYNTACTIC — bloque no es operando válido
1 + { print(1); 2 };

// TEST blk-02 — bloque en cuerpo de let (válido)
// Esperado: imprime 1 luego 2; valor del let = 2
let x = { print(1); 2 } in print(x);

// TEST blk-03 — bloque suelto como stmt global (válido)
// Esperado: imprime 42
{ print(42); };

// TEST blk-04 — let x = bloque en rhs sin in (inválido)
// Esperado: SYNTACTIC
let x = {0;1} in print(x);

// TEST blk-05 — bloque en if (válido)
// Esperado: imprime "yes"
if (true) { print("yes"); } else print("no");
```



QUISIERA ACTUALMENTE REPETIR  LA PRUEBA DE HUMO DE SI LA GRAMATICA ACTUALMENTE TIENE O NO CONFLICTOS LL1(1), PUES NO DEBE TENERLOS, PERO QUISIERA GARANTIZAR QUE NO LOS TIENE

**Respuesta:** El proyecto ya incluye `Parser/tests/ll1_table_smoke.cpp`, que construye la tabla desde `Parser/grammar/grammar.ll1` y exige `ll1_table.conflicts.empty()`. También `Compiler/main.cpp` aborta al arrancar si hay conflictos.

**Cómo repetir la prueba:**

```bash
# Compilar y ejecutar el smoke (desde la raíz del repo)
g++ -std=c++17 -Wall -I. -IParser/generator \
  Parser/tests/ll1_table_smoke.cpp Parser/generator/*.cpp \
  -o /tmp/ll1_table_smoke && /tmp/ll1_table_smoke

# Alternativa: intentar arrancar hulk (falla al cargar gramática si hay conflictos)
./hulk playground/hello.hulk
```

**Esperado:** salida `[OK] grammar builds LL(1) table without conflicts` y exit code 0. Cualquier conflicto lista `M[NoTerminal, terminal]` en stderr.



**R1 — Una definición por ámbito.** Un nombre de variable solo puede declararse una vez dentro del mismo ámbito `let`. En let anidados, está permitido declarar en let interiores nuevamente variables ya declaradas en let exteriores. La asignación destructiva `:=` reasigna un enlace existente en lugar de crear uno nuevo. Las variables globales predefinidas no pueden redefinirse con `let`.

hacer una prueba que compruebe el funcionamiento de esto

let a=2, b=3,a=2 in a+b; //esto debe dar error

let a=2, b=3 in let c =5 in {
    let a=3 in print(a+b); //aqui creo que debe imprimirse 6
    print(a); //aqui creo que debe imprimirse 2
}


otra cosa let a=2, b=3 in let c =5 in {
    let a=3 in print(a+b); //aqui creo que debe imprimirse 6
    print(a); //aqui creo que debe imprimirse 2
} de que manera se podria poner esto sin usar expresiones en bloque?? despues del let se podria poner una coma??

let a=2, b=3 in let c =5 in (let a=3 in print(a+b)), print(a);
esto de esta manera es válido??

**Respuesta:**

- `let a=2, b=3, a=2 in a+b` → **error** `REDEFINED_VARIABLE` (mismo `let`, misma lista de bindings).
- El ejemplo con bloques anidados → **válido**: inner `a=3` hace shadow; `print(a+b)` → `3+3=6`; `print(a)` fuera del inner → `a=2`.
- **Sin bloque:** usar `let` anidados encadenados con `in`:
  ```hulk
  let a=2, b=3 in let c=5 in let _ = (let a=3 in print(a+b)) in print(a);
  ```
- **`let a=2, b=3 in let c=5 in (let a=3 in print(a+b)), print(a);`** → **NO es válido** en HULK: la coma no separa expresiones secuenciales tras un `in`. El parser espera `SEMICOLON` o fin; produce **error sintáctico** en la coma.

**Casos de prueba:**

```hulk
// TEST r1-01 — duplicado en mismo let (inválido)
// Esperado: REDEFINED_VARIABLE en 'a'
let a=2, b=3, a=2 in a+b;

// TEST r1-02 — shadowing con bloque (válido)
// Esperado: imprime 6, luego 2
let a=2, b=3 in let c=5 in {
    let a=3 in print(a+b);
    print(a);
};

// TEST r1-03 — shadowing sin bloque (válido)
// Esperado: imprime 6, luego 2
let a=2, b=3 in let c=5 in let _ = (let a=3 in print(a+b)) in print(a);

// TEST r1-04 — redefinir PI (inválido)
// Esperado: REDEFINED_VARIABLE
let PI = 1 in print(PI);

// TEST r1-05 — := reasigna, no redeclara (válido)
// Esperado: imprime 2
let x = 1 in let x := 2 in print(x);

// TEST r1-06 — coma tras in (inválido)
// Esperado: SYNTACTIC en COMMA
let a=2, b=3 in let c=5 in (let a=3 in print(a+b)), print(a);
```





**R2 — Definida antes de uso.** Toda variable debe estar definida antes de aparecer en una expresión. En `let a = 1, b = a + 1 in …`, los enlaces se procesan de izquierda a derecha de modo que `b` puede referenciar `a`. La autorreferencia en un inicializador (`let x = x in …`) se rechaza. Las funciones son solo globales y no capturan variables de ámbitos `let` exteriores.

let a = 1, b = a + 1 in print(b);

let a = 1, b = a + 1 in let a = 2 in print(b);


DE AQUI QUE SIGNIFICA BIEN QUE LAS FUNCIONES SON SOLO GLOBALES Y NO CAPTURAN VARIABLES DE AMBITOS LET EXTERIORES, 

Y COMO PUEDO CREAR CASOS QUE COMPRUEBEN EL CORRECTO FUNCIONAMIENTO DE ESTO

**Respuesta:**

- `let a = 1, b = a + 1 in print(b);` → **válido**, imprime `2`.
- `let a = 1, b = a + 1 in let a = 2 in print(b);` → **válido**, imprime `2` (el `b` se ligó cuando `a` valía 1; el inner `a=2` no afecta a `b`).

**“Funciones solo globales y no capturan variables de let exteriores” significa:**
1. `function` solo puede aparecer como **sentencia de nivel superior**, nunca dentro de `let … in`, bloques ni expresiones.
2. Una función global **no ve** variables declaradas en un `let` de otra sentencia; solo ve parámetros, variables locales de su cuerpo y globals/builtins.
3. Para usar un valor de un `let` exterior, hay que **pasarlo como argumento** a la función global.

**Casos de prueba:**

```hulk
// TEST r2-01 — binding usa anterior (válido)
// Esperado: imprime 2
let a = 1, b = a + 1 in print(b);

// TEST r2-02 — shadow no afecta b ya ligado (válido)
// Esperado: imprime 2
let a = 1, b = a + 1 in let a = 2 in print(b);

// TEST r2-03 — uso antes de definir (inválido)
// Esperado: UNDEFINED_VARIABLE en 'x'
let y = x in let x = 1 in print(y);

// TEST r2-04 — autorreferencia (inválido)
// Esperado: UNDEFINED_VARIABLE en 'x'
let x = x in print(x);

// TEST r2-05 — pasar variable de let a función global (válido, NO es closure)
// Esperado: imprime 2
function inc(x) => x + 1;
let outer = 1 in print(inc(outer));

// TEST r2-06 — function dentro de let (inválido, sintaxis)
// Esperado: SYNTACTIC — function no es Expr
let outer = 1 in function f() => outer + 1 in f();

// TEST r2-07 — función que referencia variable de let sin recibirla (inválido)
// Esperado: UNDEFINED_VARIABLE en 'outer'
function bad() => outer + 1;
let outer = 1 in print(bad());
```



**R3 — Parámetros únicos y sobrecarga.** Los nombres de parámetros deben ser únicos dentro de la firma de una función o método. Varias funciones globales pueden compartir nombre si difieren en aridad; redefinir el mismo nombre con la misma aridad es un error.

PARA ESTE CASO COMO PUEDO CREAR CASOS QUE COMPRUEBEN EL CORRECTO FUCNIONAMIENTO DE LO DEFINIDO EN ESTA REGLA

COMPROBAR QUE LOS PARAMETROS DEBEN SER UNICOS DENTRO DE LA FIRMA DE UNA FUNCION O METODO

COMPROBAR QUE VARIAS FUNCIONES globales pueden compartir nombre si difieren en aridad

comprobar que redefinir el mismo nombre con la misma aridad es un error.

**Respuesta:**

- **Firma de función/método** = nombre + lista de parámetros `(nombre:tipo, …)` + tipo de retorno opcional. Los **nombres de parámetros** deben ser únicos en esa lista.
- **Sobrecarga por aridad:** `function f(x) => x;` y `function f(x,y) => x+y;` pueden coexistir.
- **Misma aridad duplicada** → `REDEFINED_FUNCTION`.

**Casos de prueba:**

```hulk
// TEST r3-01 — parámetros duplicados (inválido)
// Esperado: REDEFINED_VARIABLE en 'x'
function dup(x, x) => x;

// TEST r3-02 — parámetros distintos (válido)
// Esperado: imprime 3
function add(a, b) => a + b;
print(add(1, 2));

// TEST r3-03 — sobrecarga distinta aridad (válido)
// Esperado: imprime 3, luego 6
function suma(a, b) => a + b;
function suma(a, b, c) => a + b + c;
print(suma(1, 2));
print(suma(1, 2, 3));

// TEST r3-04 — misma aridad duplicada (inválido)
// Esperado: REDEFINED_FUNCTION
function f(x) => x;
function f(y) => y + 1;

// TEST r3-05 — método con params duplicados en clase (inválido)
// Esperado: REDEFINED_VARIABLE
type T() { f(x, x) => x; }
```



**R4 — Orden textual de declaración.** Las funciones se registran al encontrar sus declaraciones. Una llamada a una función aún no declarada en el fuente produce `UNDEFINED_FUNCTION`. Las funciones predefinidas están precargadas y no pueden redeclararse.

para esta regla como puedo hacer tests que comprueben que las funciones se registran al encontrar sus declaraciones, 

como puedo comprobar que una llamada a una funci'on aun no declarada en el fuente produce `UNDEFINED_FUNCTION`

y como puedo comprobar que las funciones predefinidas estan precargadas y no pueden redeclararse

que sucede en caso de que llame a una funcion y la declare despues, crear un caso de prueba que compruebe esto

como podemosdefinir casos en que declare funciones en scope que no sean globales, ni que sean dentro de un metodo y ver como funcionan,

**Respuesta:**

- Las funciones se **registran al procesar** cada `function` en orden de aparición en el fuente.
- Llamada a nombre inexistente → `UNDEFINED_FUNCTION`.
- Builtins (`print`, `sin`, …) precargados → **no redeclarables**.
- **Llamar antes de declarar** en el mismo archivo: según la regla R4 del manual debe ser `UNDEFINED_FUNCTION`; verificar comportamiento actual del compilador (el fixture `tests/semantic/valid/16_r4_forward_reference_in_program.hulk` sugiere que a veces se permite — conviene contrastar con `--semantic`).

**Funciones en scope no global:** en HULK **no existen** funciones locales ni lambdas en el núcleo del curso; `function` solo es `Stmt` global. Lo más cercano es pasar funciones builtin o usar métodos de objetos.

**Casos de prueba:**

```hulk
// TEST r4-01 — llamada a función inexistente (inválido)
// Esperado: UNDEFINED_FUNCTION
print(noExiste(1));

// TEST r4-02 — typo en builtin (inválido)
// Esperado: UNDEFINED_FUNCTION 'prnt'
prnt(1);

// TEST r4-03 — llamada antes de declarar (inválido según R4)
// Esperado: UNDEFINED_FUNCTION 'f' (si se respeta orden textual estricto)
let r = f(1) in print(r);
function f(n) => n;

// TEST r4-04 — declarar b antes que a que la usa (válido)
// Esperado: imprime 1
function b() => 1;
function a() => b();

// TEST r4-05 — redeclarar builtin print (inválido)
// Esperado: REDEFINED_FUNCTION
function print(x) => x;

// TEST r4-06 — mutual forward (inválido)
// Esperado: UNDEFINED_FUNCTION
function a() => b();
function b() => a();
// (llamar a() tras declarar ambas — el primero en ser *usado* antes de existir falla)
```






### 5.4 Inferencia de tipos

HULK permite omitir anotaciones de tipo en varias posiciones: variables locales en `let`, tipos de retorno de funciones y métodos, y tipos de atributos. El analizador ejecuta hasta diez pasadas de punto fijo sobre cuerpos de funciones y métodos hasta que los tipos se estabilizan o se reporta un error. Los operadores binarios numéricos, de cadena y booleanos tienen reglas de propagación que asignan tipos a referencias de identificadores sin tipo cuando se conoce el tipo del otro operando.

para esta parte dime como podemos crear variables locales en let algunas con tipo y otras sin tipo y ver su funcionamiento, crea casos de prueba de eso con tipos mixtos, variables que declaren un tipo y otras que no declaren tipo, o variables que declaren un tipo en el exterior y en el interior declaren otro tipo y ver si funciona bien, o si esto crea contradiccion, 

hacer en let anidados declaraciones de la misma varibale con tipos distintos y en alguno sin tipo y ver funcionamiento

probar a declarar funciones con tipos de retorno, tipos de metodo, y otras sin tipo y ver funcionamiento

**Respuesta:** Se pueden mezclar variables anotadas y no anotadas en el mismo `let` si los tipos son compatibles. La inferencia propaga desde literales y operadores (p. ej. `let x = 1, y = x + 2` → ambos `Number`). Si anotas un tipo y el init contradice → `TYPE_ERROR`. En `let` anidados, **shadow con distinto tipo** es posible si el init respeta la anotación de cada ámbito.

**Contradicción típica:** `let x:Number = 1 in let x:String = "a" in print(x)` → válido por shadow (son ámbitos distintos). `let x:Number = "a" in …` → inválido en el mismo binding.

**Casos de prueba:**

```hulk
// TEST inf-01 — mix anotado + inferido (válido)
// Esperado: imprime 7
let x:Number = 3, y = 4 in print(x + y);

// TEST inf-02 — inferencia por uso (válido)
// Esperado: imprime 5
let x = 2 in let y = x + 3 in print(y);

// TEST inf-03 — init incompatible con anotación (inválido)
// Esperado: TYPE_ERROR
let x:Number = "hello" in print(x);

// TEST inf-04 — shadow con tipos distintos en anidados (válido)
// Esperado: imprime "a"
let x:Number = 1 in let x = "a" in print(x);

// TEST inf-05 — función sin tipo de retorno inferido (válido)
// Esperado: imprime 21
function triple(n) => n * 3;
print(triple(7));

// TEST inf-06 — función con tipos explícitos (válido)
// Esperado: imprime 7
function add(a:Number, b:Number):Number => a + b;
print(add(3, 4));

// TEST inf-07 — método inferido en clase (válido)
// Esperado: compila
type T(x) { x = x; getX() => self.x; }
print(new T(5).getX());

// TEST inf-08 — inferencia imposible / contradictoria (inválido)
// Esperado: TYPE_ERROR tras agotar pasadas
function bad(x) => x + (x @ "a");
```





para esta parte

### 5.5 Sistema de tipos

`Types/type_info.cpp` modela `Number`, `String`, `Boolean`, `Null`, tipos objeto y tipos desconocidos o inferidos. El subtipado a lo largo de aristas de herencia se verifica mediante `conformsTo`. El operador `is` produce un `Boolean`; `as` produce el tipo objetivo cuando el cast es estáticamente posible, con comprobaciones en tiempo de ejecución emitidas por el generador de código para los casos dinámicos.

aclarame cuales son los usos que le puedo hacer al as y al is, y como los puedo hacer

**Respuesta:**

**`is`** — prueba de tipo en tiempo de compilación/ejecución; **siempre produce `Boolean`**.
- Sintaxis: `expr is Tipo`
- `Tipo` debe existir (`Number`, `String`, clase declarada, etc.).
- Ejemplos: `x is Number`, `obj is Person`, `if (x is Circle) …`

**`as`** — conversión/cast al tipo objetivo; **produce el tipo indicado** si es estáticamente posible.
- Sintaxis: `expr as Tipo`
- Casts imposibles en compile-time → `TYPE_ERROR` (p. ej. `1 as String`).
- Casts OO válidos estáticamente pero dinámicamente fallidos → código LLVM con check runtime (error en ejecución, no compilación).
- Downcast: `let s:Shape = … in let c = s as Circle in …` (tras comprobar con `is`).

**Casos de prueba:**

```hulk
// TEST type-01 — is con primitivo (válido)
// Esperado: imprime true
let x = 1 in print(x is Number);

// TEST type-02 — is con tipo inexistente (inválido)
// Esperado: UNDEFINED_TYPE 'Ghost'
let x = 1 in print(x is Ghost);

// TEST type-03 — as downcast OO (válido)
// Esperado: compila; imprime 1
type A() {}
type B() inherits A() {}
let a:A = new B() in { let b:B = a as B in print(1); };

// TEST type-04 — as imposible Number -> String (inválido)
// Esperado: TYPE_ERROR
let p = 1 as String in print(p);

// TEST type-05 — is en condición (válido)
// Esperado: imprime rama else para Superman
type Bird {} type Plane {} type Superman {}
let x = new Superman() in print(if (x is Bird) "bird" else "super");

// TEST type-06 — as tras is (patrón recomendado)
// Esperado: acceso a atributos del subtipo
type Shape { area() => 0; }
type Circle(r) inherits Shape() { r = r; area() => PI * r ^ 2; }
let s:Shape = new Circle(5) in if (s is Circle) print((s as Circle).r);
```




crear casos en el que se intente crear variable o funciones con el mismo nombre que las globales, o que se intente modificar el valor de cosas globales y ver que debe dar error

**Respuesta:** Variables globales builtin (`PI`, `E`) → no redeclarables con `let`. Funciones builtin (`print`, `sin`, `cos`, `sqrt`, …) → no redeclarables con `function` de misma aridad. **`:=` sobre PI** también debe fallar (inmutable). Modificar builtins no tiene efecto porque no existen como variables mutables.

**Casos de prueba:**

```hulk
// TEST glob-01 — redeclarar PI (inválido)
// Esperado: REDEFINED_VARIABLE
let PI = 3 in print(PI);

// TEST glob-02 — redeclarar E (inválido)
// Esperado: REDEFINED_VARIABLE
let E = 1 in print(E);

// TEST glob-03 — redeclarar sin (inválido)
// Esperado: REDEFINED_FUNCTION
function sin(x) => x;

// TEST glob-04 — usar PI sin redeclarar (válido)
// Esperado: imprime ~3.14159...
print(PI);

// TEST glob-05 — asignar a PI (inválido)
// Esperado: error semántico (variable inmutable / no definida en scope local)
PI := 3;

// TEST glob-06 — PI no puede redeclararse ni en inner let
// Esperado: REDEFINED_VARIABLE
let x = 1 in let PI = 2 in print(PI);
```

---

## Fixtures en el repo

Los casos anteriores están implementados como archivos `.hulk` en [`playground/notas_tests/`](notas_tests/) (93 fixtures en `valid/` e `invalid/`).

Verificación:

```bash
bash playground/verify_notas_tests.sh
```

Regenerar expectativas del manifest tras cambiar el compilador:

```bash
python3 scripts/gen_notas_manifest.py
```











1. **`function` solo como sentencia global** — no puedes escribir `let x = 1 in function f() => x + 1 in ...` (error **sintáctico**: `function` no es expresión).



hay que hacer un caso que me compruebe que no se permita esto




para este caso

| Detectar ciclos | `A inherits B inherits A` o `A inherits A` | `validateInheritanceChain`: recorre padres con un `set` de visitados |


probar con ciclos mas grandes y complejos a ver si los encuentra bien



para este otro caso 

**`new` — validación**

**Uso en el lenguaje:** `new NombreTipo(arg1, arg2, ...)` crea una instancia.


crear casos en el que se creen nuevos tipos

y comprobar creacion de elementos que hereden de otros que hereden de otros y poner un error de codigo en el 3ero del que hereden