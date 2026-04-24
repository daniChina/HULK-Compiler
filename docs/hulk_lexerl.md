FUncionamiento de hulk_lexer.l



Perfecto, esa parte es clave del lexer.

## Opciones (`%option`)
En [hulk_lexer.l](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l):

- `%option yyclass="HulkLexer"`  
  Le dice a flex++ que genere `yylex()` como método de tu clase `HulkLexer`.
- `%option noyywrap`  
  Evita depender de `yywrap()` al llegar al fin de archivo.
- `%option never-interactive`  
  Trata la entrada como no interactiva (mejor para archivos/pipes).
- `%option nodefault`  
  Si no hay regla que haga match, no usa acción por defecto silenciosa; tú controlas eso con la regla `.`.

## Prólogo (`%{ ... %}`)
Ese bloque se copia tal cual al inicio del `.cpp` generado por flex.  
Ahí pones cosas que el código generado necesita conocer antes de compilar reglas:

- `#include "hulk_lexer.h"` para que exista `HulkLexer`, `TokenType`, etc.
- macros/funciones auxiliares usadas dentro de acciones.

## Macro interna `UPD(t)`
```cpp
#define UPD(t) do { column_ += yyleng; \
                    return static_cast<int>(TokenType::t); } while(0)
```

Qué hace exactamente:

1. `column_ += yyleng;`  
   Avanza la columna según cuántos caracteres consumió el token.
2. `return static_cast<int>(TokenType::t);`  
   Devuelve el token correspondiente como `int` (porque `yylex()` devuelve `int`).
3. `do { ... } while(0)`  
   Patrón clásico para que la macro se comporte como una sola sentencia segura en cualquier contexto (`if/else`, etc.).

Ejemplo real:
```cpp
"if" { UPD(KEYWORD_IF); }
```
se expande a:
```cpp
do {
  column_ += yyleng;
  return static_cast<int>(TokenType::KEYWORD_IF);
} while(0);
```

## ¿Qué es `yyleng`?
`yyleng` es una variable que flex provee automáticamente:  
es la **longitud del lexema reconocido** por la regla actual.

- Si matchea `"if"`, `yyleng == 2`
- Si matchea `">="`, `yyleng == 2`
- Si matchea un identificador `variable123`, `yyleng == 11`

Relacionada: `yytext` contiene el texto exacto matcheado.

## ¿Qué es `TokenType::t`?
`TokenType` es tu `enum class` en [hulk_lexer.h](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.h).  
`::t` no es literal fijo: `t` es parámetro de la macro y se sustituye textual.

- `UPD(PLUS)` → `TokenType::PLUS`
- `UPD(KEYWORD_FOR)` → `TokenType::KEYWORD_FOR`

O sea, `UPD` evita repetir el mismo código para muchos tokens.








Una **macro interna** aquí es un atajo del preprocesador C/C++ (`#define`) usado dentro de las acciones de Flex para no repetir código.

## 1) Macro interna (`UPD`)
En tu lexer:

```cpp
#define UPD(t) do { column_ += yyleng; \
                    return static_cast<int>(TokenType::t); } while(0)
```

Se usa en reglas como `"if" { UPD(KEYWORD_IF); }`.

Qué hace:
- suma a `column_` la longitud del lexema (`yyleng`)
- retorna el token correspondiente (`TokenType::KEYWORD_IF`, etc.) como `int`
- el `do { } while(0)` hace que la macro sea segura como “una sola sentencia”.

Se utiliza cuando una regla solo necesita: “actualizar columna + devolver token”.

---

## 2) Macros de patrones (Flex)
Esto:

```lex
DIGIT       [0-9]
ALPHA       [a-zA-Z]
ALNUM_      [a-zA-Z0-9_]
INT         {DIGIT}+
FLOAT       {DIGIT}+\.{DIGIT}+
IDENT       {ALPHA}{ALNUM_}*
```

son **alias de regex** de Flex (no son `#define` de C++).  
Sirven para construir patrones reutilizables y legibles.

Ejemplos:
- `{INT}` = uno o más dígitos
- `{FLOAT}` = dígitos, punto, dígitos
- `{IDENT}` = letra seguida de letras/números/`_`

Se usan en reglas reales:
```lex
{FLOAT} { ... }
{INT}   { ... }
{IDENT} { ... }
```

---

## 3) Reglas 1. Espacios
Código:

```lex
[ \t\r]+      { column_ += yyleng; }
\n            { ++line_; column_ = 1; }
```

Cómo funciona:
- `[ \t\r]+` consume espacios/tab/retorno de carro consecutivos; no retorna token, solo avanza columna.
- `\n` consume salto de línea; incrementa línea y reinicia columna a 1.

Cuándo se usan:
- siempre que el lexer encuentre whitespace.
- se ejecutan antes de seguir buscando el siguiente token “real”.
- por diseño, whitespace queda **ignorado** (no llega al parser).

---

## 4) Reglas 2. Comentarios
Código:

```lex
"//"[^\n]*    { column_ += yyleng; }
```
- consume comentario de línea desde `//` hasta antes de `\n`
- no devuelve token; se ignora su contenido.

Código de bloque:

```lex
"/*" { ... yyinput() ... }
```

Cómo funciona:
- al ver `/*`, entra en un bucle leyendo carácter por carácter con `yyinput()`
- actualiza línea/columna manualmente
- termina cuando detecta secuencia `*/`
- si llega EOF (`c2 == 0`) antes de cerrar, reporta error: comentario sin cerrar.

Cuándo se usa:
- justo cuando aparece `/*`; como puede abarcar varias líneas, no basta regex simple y por eso se usa lógica manual.

---

## Resumen operativo
Estas reglas se ejecutan **durante `yylex()`**, cada vez que se pide un token:
1. Flex toma el texto pendiente.
2. Elige la regla que mejor matchee.
3. Ejecuta su acción.
4. Si la acción no retorna token (espacios/comentarios), sigue internamente.
5. Cuando una regla retorna, `yylex()` entrega ese token al `main`/parser.





En `hulk_lexer.l`, esas secciones son reglas que se ejecutan dentro de `yylex()` cuando el texto de entrada coincide con su patrón.

## 3) Literales de cadena
Regla inicia al ver `"`:

- patrón: `\"`
- acción: entra en un bucle leyendo carácter a carácter con `yyinput()`
- acumula contenido en `std::string buf`
- soporta escapes: `\"`, `\\`, `\n`, `\t`, `\r`
- si encuentra fin de línea o EOF sin cerrar `"`, reporta error de cadena sin cerrar
- al terminar:
  - guarda `buf` en `sem.str_val`
  - retorna `STRING_LITERAL`

Cuándo se usa: cada vez que el próximo token empieza con comillas dobles.

## 4) Números
Hay dos reglas:

- `{FLOAT}`: números con punto (ej. `3.14`)
  - usa `std::stof(yytext)`
  - guarda en `sem.num_val`
  - retorna `NUMBER_LITERAL`
- `{INT}`: enteros (ej. `42`)
  - usa `std::atoi(yytext)` y lo pasa a `float`
  - guarda en `sem.num_val`
  - retorna `NUMBER_LITERAL`

Cuándo se usa: cuando el texto coincide con patrón de número.

## 5) Booleanos
Reglas directas:

- `"true"`:
  - `sem.bool_val = true`
  - retorna `BOOLEAN_LITERAL`
- `"false"`:
  - `sem.bool_val = false`
  - retorna `BOOLEAN_LITERAL`

Cuándo se usa: cuando aparecen exactamente esas palabras.

## Qué es `yytext`
`yytext` es el lexema actual reconocido por Flex (el texto exacto que hizo match en esa regla).  
Ejemplo: si se reconoce `123`, entonces `yytext == "123"`.

## Qué es `UPD`
`UPD(t)` es una macro auxiliar:

- suma `yyleng` a `column_` (avanza columna)
- retorna `TokenType::t` como `int`

Se usa para reglas simples (keywords/símbolos) para no repetir código.

## Qué son `sem.bool_val`, `sem.num_val`, `sem.str_val`
`sem` es el “valor semántico” del token actual (`SemanticValue` en `hulk_lexer.h`):

- `sem.str_val`: texto de identificadores y strings
- `sem.num_val`: valor numérico de tokens numéricos
- `sem.bool_val`: valor booleano (`true/false`)

Nota: escribiste `semt.str_val`; en tu código correcto es `sem.str_val`.





Perfecto. En esas partes de `hulk_lexer.l`, el comportamiento depende de cómo Flex decide qué regla aplicar en cada posición de entrada.

## 6) Palabras clave
Reglas como:

- `"if" { UPD(KEYWORD_IF); }`
- `"while" { UPD(KEYWORD_WHILE); }`
- etc.

Qué hacen:
- si el texto pendiente empieza exactamente con esa palabra, consumen esos caracteres
- `UPD(...)` actualiza `column_` con `yyleng` y retorna el token correspondiente (`KEYWORD_IF`, etc.)

Cuándo se usan:
- cuando en la entrada aparece exactamente una keyword definida.

Punto importante:
- están **antes** de `{IDENT}`, así `if` sale como keyword y no como identificador.

---

## 8) Identificadores
Regla:

- `{IDENT} { column_ += yyleng; sem.str_val = yytext; return IDENTIFIER; }`

donde `IDENT` es:
- `{ALPHA}{ALNUM_}*` (letra seguida de letras/dígitos/_)

Qué hace:
- reconoce nombres de variables/funciones/tipos
- guarda el texto en `sem.str_val`
- devuelve `IDENTIFIER`

Cuándo se usa:
- cuando matchea patrón de identificador **y no ganó antes una regla más específica** (como keyword).

---

## 11) EOF
Regla:

- `<<EOF>> { return EOF_TOKEN; }`

Qué hace:
- cuando ya no queda más entrada en el stream, devuelve token de fin de archivo.

Cuándo se usa:
- al final real del input (archivo o stdin), para que el `while` de `main` termine.

---

## 12) Carácter desconocido
Regla:

- `. { ... return UNKNOWN; }`

Qué significa `.` en Flex:
- “cualquier carácter” (normalmente excepto `\n`; en tu archivo `\n` ya tiene regla propia).

Qué hace:
- consume 1 carácter que no coincidió con ninguna regla anterior válida
- reporta error léxico con línea/columna
- guarda ese carácter en `sem.str_val`
- devuelve `UNKNOWN`

Cuándo se usa:
- como **fallback** cuando nada más matchea en esa posición.

---

## Cómo decide Flex “con quién matchea”
En cada llamada interna de escaneo, Flex aplica esta política:

1. Intenta todas las reglas desde la posición actual.
2. Elige el match **más largo**.
3. Si hay empate de longitud, gana la regla que aparece **antes** en el archivo.

Por eso:
- `if` puede matchear keyword `"if"` y también `{IDENT}`, ambos longitud 2, gana `"if"` porque está antes.
- para un símbolo raro no cubierto (ej. `$` si no hay regla para `$`), ninguna regla específica matchea, entonces entra `.` y devuelve `UNKNOWN`.

En resumen, la regla 12 “sabe” con quién matchea porque es la última red de seguridad: se ejecuta solo cuando las anteriores no pudieron ganar.