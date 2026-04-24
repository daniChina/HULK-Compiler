# Parser Contract

Este documento fija la interfaz que el parser consumira desde el lexer.

## 1. Mantener `IDENTIFIER`

Se mantiene `IDENTIFIER` como nombre del token de identificadores.

Motivo:

- ya esta presente en el lexer actual
- es claro semantica y sintacticamente
- evita renombrados innecesarios ahora

No hay necesidad tecnica de pasarlo a `IDENT` si todo el proyecto puede mantenerse consistente con `IDENTIFIER`.

## 2. Estructura `Token`

La estructura definida en `Parser/token.hpp` es:

```cpp
struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int col;
};
```

## 3. Para que es necesaria

El parser necesita mas informacion que solo el tipo del token.

### `type`

Le dice al parser que terminal recibio realmente.

Ejemplos:

- `IDENTIFIER`
- `IF`
- `LPAREN`
- `TRUE`

### `lexeme`

Guarda el texto original exacto.

Ejemplos:

- para `IDENTIFIER`, sirve para saber si el nombre fue `x`, `Point`, `print`, etc.
- para `NUMBER_LITERAL`, permite conservar `42`, `3.14`, etc.
- para `STRING_LITERAL`, permite conservar el contenido original leido

### `line` y `col`

Permiten reportar errores del parser con ubicacion real.

Ejemplo:

- "se esperaba `RPAREN` en linea 4, columna 17"

## 4. Conjunto final de tokens expuestos al parser

El conjunto final no tiene por que copiar 1:1 los nombres internos del lexer.

Lo correcto es exponer al parser un conjunto estable, limpio y orientado a la gramatica.

Por eso en `Parser/token.hpp` los nombres finales quedaron como:

- `IDENTIFIER`, `NUMBER_LITERAL`, `STRING_LITERAL`, `TRUE`, `FALSE`
- `IF`, `ELIF`, `ELSE`, `WHILE`, `FOR`, `FUNCTION`, `TYPE`, `PROTOCOL`, `DEF`, `LET`, `IN`, `NEW`, `INHERITS`, `SELF`, `BASE`, `IS`, `AS`, `AND`, `OR`
- delimitadores y operadores
- `EOF_TOKEN`, `UNKNOWN`

## 5. Adaptador lexer -> TokenList

El adaptador ahora vive en:

- `Parser/token_adapter.hpp`
- `Parser/token_adapter.cpp`

Su papel es convertir la salida del lexer en una `TokenList`.

Aunque ahora el lexer ya usa los mismos nombres de token que el parser, el adaptador sigue siendo necesario por tres razones:

1. materializa la secuencia completa de tokens
2. conserva `lexeme`, `line` y `col` en cada `Token`
3. desacopla el parser de la API interna de Flex

## 6. Separacion entre Lexer y Parser

Todo este trabajo se deja en `Parser/` para no mezclar:

- implementacion del lexer en `Lexer/`
- contrato del parser y trabajo sintactico en `Parser/`

Esa separacion te va a ayudar cuando empieces a construir:

1. el adaptador lexer -> lista de tokens
2. el parser recursivo o LL(1)
3. el AST
