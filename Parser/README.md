# Parser Folder

Esta carpeta contiene el contrato que va a consumir el parser y todo el trabajo posterior de analisis sintactico.

## Separacion respecto a Lexer

- `Lexer/` se encarga de reconocer lexemas y producir tipos de token desde Flex
- `Parser/` define los tokens que quiere ver el parser y, mas adelante, construira la logica sintactica sobre ellos

La idea es que el parser no dependa de detalles internos del lexer mas de lo necesario.

## Contrato actual del parser

El archivo central es:

- `Parser/core/token.hpp`
- `Parser/core/token_adapter.hpp`
- `Parser/core/token_adapter.cpp`
- `Parser/core/parse_error.hpp`
- `Parser/core/token_stream.hpp`
- `Parser/core/token_stream.cpp`
- `Parser/ast/expr.hpp`
- `Parser/ast/expr.cpp`
- `Parser/ast/cst_nodes.hpp`
- `Parser/ast/cst_nodes.cpp`
- `Parser/ast/cst_to_ast.hpp`
- `Parser/ast/cst_to_ast.cpp`
- `Parser/syntax/parser.hpp`
- `Parser/syntax/parser.cpp`
- `Parser/syntax/ll1_parser.hpp`
- `Parser/syntax/ll1_parser.cpp`
- `Compiler/main.cpp`
- `Parser/generator/production.hpp`
- `Parser/generator/grammar_reader.hpp`
- `Parser/generator/grammar_reader.cpp`
- `Parser/generator/first_follow.hpp`
- `Parser/generator/first_follow.cpp`
- `Parser/generator/ll1_table.hpp`
- `Parser/generator/ll1_table.cpp`
- `Parser/tests/parser_phase2_smoke.cpp`

Ahi se define:

- `parser::TokenType`: conjunto final de nombres de token expuestos al parser
- `parser::Token`: estructura de datos de un token individual
- `parser::TokenList`: alias para una secuencia de tokens
- funciones para convertir la salida del lexer en `TokenList`
- un error sintactico basico (`ParseError`)
- una secuencia navegable de tokens (`TokenStream`)
- un AST minimo de expresiones (`Expr` y derivados)
- nodos de CST para el parser generado
- conversion CST -> AST del subconjunto actual
- un parser recursivo inicial para expresiones primarias (`Parser`)
- un parser predictivo LL(1) generico (`Ll1Parser`)
- un driver de extremo a extremo lexer -> parser -> CST/AST
- una representacion interna de producciones LL(1)
- un lector de `grammar.ll1`
- calculo de conjuntos FIRST y FOLLOW
- construccion y validacion de la tabla LL(1)

## Organizacion actual

La carpeta `Parser/` ahora queda separada por responsabilidad:

- `Parser/core/`: contrato de tokens, adaptador lexer -> parser, error sintactico y `TokenStream`
- `Parser/ast/`: nodos de AST manual y nodos de CST del parser LL(1)
- `Parser/grammar/`: fuente formal de la gramatica LL(1)
- `Parser/generator/`: lectura y procesamiento de la gramatica
- `Parser/syntax/`: parser recursivo manual y parser LL(1)
- `Parser/tests/`: pruebas pequenas aisladas del parser
- `Parser/README.md`, `Parser/plan_parser.md`, `Parser/explicacion.md`: documentacion y plan de trabajo

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

- `Parser/core/token_adapter.hpp`
- `Parser/core/token_adapter.cpp`

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

- `Parser/core/parse_error.hpp`
- `Parser/core/token_stream.hpp`
- `Parser/core/token_stream.cpp`

Estas piezas aportan la infraestructura minima para empezar un parser:

- `current()`
- `peek()`
- `advance()`
- `is(type)`
- `match(type)`
- `consume(type, mensaje)`

Con eso ya se puede escribir despues un parser recursivo sin tocar la capa del lexer.

## Fase 2 del parser

La fase 2 queda iniciada con:

- `Parser/ast/expr.hpp`
- `Parser/ast/expr.cpp`
- `Parser/syntax/parser.hpp`
- `Parser/syntax/parser.cpp`
- `Parser/tests/parser_phase2_smoke.cpp`

Por ahora replica el nivel mas pequeno de la gramatica de referencia:

- `Expr -> Primary`
- `Primary -> NUMBER | STRING | TRUE | FALSE | IDENTIFIER | LPAREN Expr RPAREN`

Los nodos soportados en esta etapa son:

- `NumberExpr`
- `StringExpr`
- `BoolExpr`
- `IdentifierExpr`
- `GroupedExpr`

Y la prueba smoke cubre:

- `42;`
- `"hola";`
- `true;`
- `x;`
- `(42);`

## Fase 3 del parser

La fase 3 extiende el parser recursivo de `Parser/syntax/parser.cpp` para soportar precedencia por niveles.

Los niveles implementados son:

- `parse_or()`
- `parse_and()`
- `parse_comparison()`
- `parse_concat()`
- `parse_add()`
- `parse_mul()`
- `parse_power()`
- `parse_unary()`
- `parse_primary()`

Los nodos nuevos del AST son:

- `UnaryExpr`
- `BinaryExpr`

La prueba smoke de esta fase vive en:

- `Parser/tests/parser_phase3_smoke.cpp`

Y cubre:

- `1 + 2 * 3;`
- `a and b or c;`
- `"a" @ "b";`
- `x < y == z;`
- `-x;`
- `(1 + 2) * 3;`
- `2 ^ 3 ^ 4;`

## Fase 4 del parser

La fase 4 queda iniciada con una gramatica LL(1) formal en:

- `Parser/grammar/grammar.ll1`

Por ahora esa gramatica cubre exactamente el subconjunto ya validado en el parser manual:

- una sentencia de expresion terminada en `SEMICOLON`
- primarias
- postfix encadenado (`call` y acceso con `.`)
- unarios
- potencia con asociatividad derecha
- multiplicacion y division
- suma y resta
- concatenacion
- comparacion
- `and`
- `or`

Las decisiones activas en esta primera version son:

- los terminales usan exactamente los nombres de `Parser/core/token.hpp`
- `ε` representa epsilon
- la entrada actual es `Program -> ExprStmt EOF_TOKEN`

El siguiente paso natural sobre esta base es implementar:

- `Parser/generator/production.hpp`
- `Parser/generator/grammar_reader.*`

## Fase 5 del parser

La fase 5 queda iniciada con:

- `Parser/generator/production.hpp`
- `Parser/generator/grammar_reader.hpp`
- `Parser/generator/grammar_reader.cpp`
- `Parser/tests/grammar_reader_smoke.cpp`

Estas piezas permiten:

- representar producciones LL(1) de forma neutral
- cargar `grammar.ll1` desde archivo o stream
- identificar simbolo inicial
- separar no terminales y terminales
- materializar epsilon como produccion con `rhs` vacio

La prueba smoke de esta fase verifica:

- lectura correcta del simbolo inicial
- carga de producciones importantes
- deteccion de terminales y no terminales
- lectura correcta de alternativas epsilon

## Fase 6 del parser

La fase 6 queda iniciada con:

- `Parser/generator/first_follow.hpp`
- `Parser/generator/first_follow.cpp`
- `Parser/tests/first_follow_smoke.cpp`

Estas piezas permiten:

- calcular `FIRST(X)` para simbolos terminales y no terminales
- calcular `FIRST(alpha)` para secuencias de simbolos
- calcular `FOLLOW(A)` para cada no terminal
- manejar epsilon de forma explicita
- dejar lista la informacion base para construir la tabla LL(1)

La prueba smoke de esta fase verifica:

- `FIRST(Primary)`
- `FIRST(UnaryExpr)`
- `FIRST(PostfixTail)` con `LPAREN`, `DOT` y epsilon
- epsilon en `FIRST(OrExprTail)`
- simbolos importantes dentro de `FOLLOW(Expr)`
- simbolos importantes dentro de `FOLLOW(Primary)`
- `FIRST` de una secuencia de simbolos

## Fase 7 del parser

La fase 7 queda iniciada con:

- `Parser/generator/ll1_table.hpp`
- `Parser/generator/ll1_table.cpp`
- `Parser/tests/ll1_table_smoke.cpp`

Estas piezas permiten:

- construir la tabla predictiva `M[A, a]`
- llenar celdas usando `FIRST(alpha)`
- llenar celdas epsilon usando `FOLLOW(A)`
- detectar conflictos cuando dos producciones compiten por la misma celda
- dejar lista la informacion que consumira el parser LL(1)

La prueba smoke de esta fase verifica:

- ausencia de conflictos en la gramatica actual
- celdas concretas para `Primary`
- celdas concretas para `OrExprTail`
- celdas concretas para `PowerExprTail`
- celdas concretas para `PostfixTail`

## Fase 8 del parser

La fase 8 queda iniciada con:

- `Parser/syntax/ll1_parser.hpp`
- `Parser/syntax/ll1_parser.cpp`
- `Parser/tests/ll1_parser_smoke.cpp`

Estas piezas permiten:

- consumir `TokenList` usando la tabla LL(1)
- mapear `TokenType` al vocabulario terminal de la gramatica
- expandir no terminales con lookahead
- consumir terminales esperados
- reportar errores sintacticos cuando no exista entrada valida en la tabla
- guardar una derivacion simple de las producciones aplicadas

La prueba smoke de esta fase verifica:

- aceptacion de una expresion valida
- rechazo de una expresion incompleta
- aceptacion de una cadena postfix valida (`a.b(c);`)
- rechazo de un postfix mal formado (`a.;`)

## Fase 9 del parser

La fase 9 queda iniciada con:

- `Parser/ast/cst_nodes.hpp`
- `Parser/ast/cst_nodes.cpp`
- integracion del CST dentro de `Parser/syntax/ll1_parser.cpp`
- `Parser/tests/ll1_cst_smoke.cpp`

Estas piezas permiten:

- modelar nodos terminales y no terminales de un CST
- construir el arbol de derivacion durante el parseo LL(1)
- materializar epsilon como hijo explicito cuando una produccion deriva vacio
- asociar tokens reales a los nodos terminales consumidos
- inspeccionar la forma del arbol en pruebas

La prueba smoke de esta fase verifica:

- existencia del nodo raiz del CST
- simbolo raiz `Program`
- presencia de nodos intermedios como `ExprStmt`
- almacenamiento del token consumido en un nodo terminal
- rechazo correcto del caso sintacticamente invalido
- presencia de nodos `PostfixExpr`, `DOT` y `LPAREN` en una cadena postfix

## Fase 10 del parser

La fase 10 queda iniciada con:

- `Parser/ast/cst_to_ast.hpp`
- `Parser/ast/cst_to_ast.cpp`
- `Parser/tests/cst_to_ast_smoke.cpp`

Estas piezas permiten:

- convertir la raiz `Program` del CST al AST de expresiones ya existente
- eliminar nodos puramente sintacticos como `Expr`, `Tail`, `SEMICOLON` y `EOF_TOKEN`
- reconstruir precedencia y asociatividad desde la forma del CST
- obtener nodos `NumberExpr`, `StringExpr`, `BoolExpr`, `IdentifierExpr`, `GroupedExpr`, `UnaryExpr`, `BinaryExpr`, `CallExpr` y `GetAttrExpr`
- convertir cadenas postfix del CST (`(...)` y `.IDENTIFIER`) a AST real

La prueba smoke de esta fase verifica:

- conversion correcta de `1 + 2 * 3;`
- conversion correcta de agrupacion y potencia
- conversion correcta de llamada `f(1, 2);`
- conversion correcta de cadena `a.b(c);`

## Flujo de extremo a extremo

Ya existe un punto de entrada real para ejecutar:

- lexer
- lectura de gramatica
- calculo de `FIRST/FOLLOW`
- construccion de tabla LL(1)
- parseo LL(1)
- impresion opcional de tokens, CST y AST

El driver vive en:

- `Compiler/main.cpp`

Uso general:

```bash
./hulk_parser [--tokens] [--cst] [--ast] [archivo.hulk]
```

Si no se pasa archivo, lee desde `stdin`.

Un ejemplo minimo compatible con la gramatica actual vive en:

- `Parser/tests/valid_expr_pipeline.hulk`
