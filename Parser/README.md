# Parser Folder

Esta carpeta contiene el contrato que va a consumir el parser y todo el trabajo posterior de analisis sintactico.

## Separacion respecto a Lexer

- `Lexer/` se encarga de reconocer lexemas y producir tipos de token desde Flex
- `Parser/` define los tokens que quiere ver el parser y, mas adelante, construira la logica sintactica sobre ellos

La idea es que el parser no dependa de detalles internos del lexer mas de lo necesario.

## Contrato actual del parser

El archivo central es:

- `Parser/token.hpp`
- `Parser/token_adapter.hpp`
- `Parser/token_adapter.cpp`
- `Parser/parse_error.hpp`
- `Parser/token_stream.hpp`
- `Parser/token_stream.cpp`

Ahi se define:

- `parser::TokenType`: conjunto final de nombres de token expuestos al parser
- `parser::Token`: estructura de datos de un token individual
- `parser::TokenList`: alias para una secuencia de tokens
- funciones para convertir la salida del lexer en `TokenList`
- un error sintactico basico (`ParseError`)
- una secuencia navegable de tokens (`TokenStream`)

## Decisiones activas

- se mantiene `IDENTIFIER` como nombre del token de identificadores
- los builtins (`print`, `sin`, `cos`, `range`, `PI`, `E`, etc.) llegan al parser como `IDENTIFIER`
- `true` y `false` llegan al parser como `TRUE` y `FALSE`

## Por que hace falta `Token { type, lexeme, line, col }`

El parser no debe trabajar directamente con la clase interna de Flex ni con variables globales del escaner.

Necesita una representacion estable y simple de cada token:

- `type`: que clase de token es
- `lexeme`: el texto exacto leido del programa fuente
- `line`: linea donde empieza el token
- `col`: columna donde empieza el token

Eso permite:

1. desacoplar lexer y parser
2. hacer debugging del flujo de tokens
3. reportar errores sintacticos con ubicacion real
4. construir mas adelante AST, recovery y mensajes de error mejores

## Conjunto final de nombres de token expuestos al parser

Se eligio un conjunto final pensando en el parser, no en la implementacion interna actual del lexer.

### Literales y nombres

- `IDENTIFIER`
- `NUMBER_LITERAL`
- `STRING_LITERAL`
- `TRUE`
- `FALSE`

### Keywords

- `IF`, `ELIF`, `ELSE`
- `WHILE`, `FOR`
- `FUNCTION`, `TYPE`, `PROTOCOL`, `DEF`
- `LET`, `IN`
- `NEW`, `INHERITS`
- `SELF`, `BASE`
- `IS`, `AS`
- `AND`, `OR`

### Delimitadores y puntuacion

- `LPAREN`, `RPAREN`
- `LBRACE`, `RBRACE`
- `LBRACKET`, `RBRACKET`
- `COMMA`, `SEMICOLON`, `DOT`, `COLON`

### Operadores

- `PLUS`, `MINUS`, `STAR`, `SLASH`, `CARET`
- `TILDE`, `BANG`
- `ASSIGN`, `ARROW`
- `CONCAT`, `CONCAT_WS`
- `EQUAL`, `EQUAL_EQUAL`, `BANG_EQUAL`
- `LESS`, `LESS_EQUAL`, `GREATER`, `GREATER_EQUAL`

### Tecnicos

- `EOF_TOKEN`
- `UNKNOWN`

## Adaptador lexer -> TokenList

El adaptador ya vive en:

- `Parser/token_adapter.hpp`
- `Parser/token_adapter.cpp`

Su trabajo es:

1. ejecutar `HulkLexer`
2. leer cada token emitido por el lexer
3. construir un `parser::Token` con:
   - `type`
   - `lexeme`
   - `line`
   - `col`
4. devolver una `TokenList`

Aunque los nombres del lexer y del parser ya coinciden, el adaptador sigue siendo necesario porque:

- el parser no debe depender directamente de Flex
- el parser necesita una lista materializada de tokens
- el parser necesita acceder al `lexeme` y a la posicion de cada token

## Fase 1 del parser

La fase 1 ya queda soportada con:

- `Parser/parse_error.hpp`
- `Parser/token_stream.hpp`
- `Parser/token_stream.cpp`

Estas piezas aportan la infraestructura minima para empezar un parser:

- `current()`
- `peek()`
- `advance()`
- `is(type)`
- `match(type)`
- `consume(type, mensaje)`

Con eso ya se puede escribir despues un parser recursivo sin tocar la capa del lexer.
