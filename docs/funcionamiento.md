**Qué está implementado ahora**
El proyecto está en fase de **lexer** (análisis léxico), no parser/AST/semántica todavía.

- Define tipos de token en [hulk_lexer.h#L24](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.h#L24).
- Implementa reglas Flex en [hulk_lexer.l#L49](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L49).
- Tiene un driver de prueba en [main.cpp#L5](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L5) que solo imprime tokens.

Reconoce hoy:

- espacios/saltos de línea y mantiene posición `line_`, `column_` ([hulk_lexer.l#L52](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L52))
- comentarios `//...` y `/*...*/` ([hulk_lexer.l#L56](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L56))
- strings con escapes básicos ([hulk_lexer.l#L78](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L78))
- números `int`/`float` como `NUMBER_LITERAL` ([hulk_lexer.l#L110](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L110))
- booleanos `true/false` ([hulk_lexer.l#L119](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L119))
- keywords, builtins, identificadores ([hulk_lexer.l#L125](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L125), [hulk_lexer.l#L146](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L146), [hulk_lexer.l#L158](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L158))
- operadores y puntuación (`:=`, `=>`, `==`, `@`, `+`, `-`, etc.) ([hulk_lexer.l#L162](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L162))
- `EOF_TOKEN` y `UNKNOWN` ([hulk_lexer.l#L193](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L193))

**Cómo funciona `main.cpp`**
Flujo exacto:

1. Arranca en `main(int argc, char* argv[])` ([main.cpp#L5](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L5)).
2. Si hay argumento (`argc > 1`), intenta abrir ese archivo ([main.cpp#L16](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L16)).
3. Si no hay argumento, usa `stdin` ([main.cpp#L13](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L13)).
4. Crea `HulkLexer lexer(*input, std::cerr)` ([main.cpp#L25](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L25)).
5. Llama repetidamente `lexer.yylex()` hasta `EOF_TOKEN` ([main.cpp#L30](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L30)).
6. Por cada token imprime nombre y, si aplica, valor semántico (`sem.str_val`, `sem.num_val`, `sem.bool_val`) ([main.cpp#L38](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L38)).
7. Al final imprime `[EOF]` y sale con `0` ([main.cpp#L56](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L56)).

**Qué recibe `main.cpp`**
Recibe dos formas de entrada:

- `argv[1]`: ruta de archivo `.hulk` opcional.
- `stdin`: si no se pasa archivo.

Además, internamente el lexer recibe stream de salida de errores (`std::cerr`) para reportar:

- comentario sin cerrar ([hulk_lexer.l#L66](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L66))
- cadena sin cerrar ([hulk_lexer.l#L85](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L85))
- carácter desconocido ([hulk_lexer.l#L198](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l#L198))

**Qué devuelve**
Hay dos “salidas” distintas:

- Como proceso (`return` de `main`):
  - `0` si todo corre.
  - `1` si no pudo abrir archivo ([main.cpp#L19](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp#L19)).
- Como resultado visible:
  - escribe en `stdout` la lista de tokens.
  - escribe en `stderr` errores léxicos.

`yylex()` devuelve `int` con el código numérico del token (enum `TokenType`), y `main` lo transforma a nombre con `HulkLexer::token_name(...)` ([hulk_lexer.h#L105](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.h#L105)).

Si quieres, en el próximo paso te hago un recorrido línea por línea sobre un fragmento de `example.hulk` y te digo exactamente qué tokens produciría.









Muy buena pregunta. Te lo aterrizo al flujo real de tu proyecto.

`yylex()` se ejecuta en el objeto `lexer` creado en [main.cpp:25](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp:25), y lo llama este `while` en [main.cpp:30](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp:30):

```cpp
while ((tok = lexer.yylex()) != EOF_VAL) { ... }
```

## Quién llama a `yylex`
Ahora mismo lo llama `main` (no parser).  
En compiladores completos, normalmente lo llama el parser (`yyparse`), pero en tu estado actual el consumidor es `main.cpp`.

## Dónde “vive” `yylex`
- Declaración: [hulk_lexer.h:100](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.h:100)
- Implementación: la genera `flex++` al compilar `hulk_lexer.l` (en `hulk_lexer.cpp` generado).

Con `%option yyclass="HulkLexer"` [hulk_lexer.l:22](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l:22), `yylex` queda como método de `HulkLexer`.

## Cómo Flex toma “texto pendiente”
El lexer tiene un buffer interno asociado al `istream` que le pasaste en el constructor (`archivo` o `stdin`) [main.cpp:13](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp:13).  
“Texto pendiente” = la parte de ese buffer que aún no se consumió.

Cada llamada a `yylex()` continúa desde ahí, no desde el principio.

## Cómo elige la regla
Dentro de `yylex`, Flex aplica sus reglas del bloque `%% ... %%` de [hulk_lexer.l:49](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l:49) con esta lógica:

1. Busca coincidencias desde la posición actual.
2. Elige la coincidencia más larga (longest match).
3. Si hay empate en longitud, gana la regla que aparece antes en el archivo.
4. Ejecuta su acción C++.

Por eso keywords como `"if"` están antes de `{IDENT}`: para que `if` salga como keyword y no como identificador.

## A quién entrega `yylex()` el retorno
Se lo entrega al **llamador inmediato**, que aquí es `main`:

- retorno de la regla `return static_cast<int>(TokenType::...)` en `hulk_lexer.l`  
- llega a `tok` en [main.cpp:30](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp:30)
- luego `main` imprime nombre/valor del token [main.cpp:33](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/main.cpp:33)

## Qué pasa con reglas que no retornan
Ejemplo espacios/comentarios [hulk_lexer.l:52](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l:52), [hulk_lexer.l:56](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l:56):

- ejecutan acción (actualizan columna/línea, ignoran comentario)
- no hacen `return`
- entonces `yylex()` sigue internamente buscando la próxima regla hasta encontrar una que sí retorne token o `EOF`.

Si quieres, te hago el recorrido exacto de 1 línea de `example.hulk` paso a paso mostrando qué regla dispara en cada fragmento.