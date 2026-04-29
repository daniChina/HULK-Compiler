**Vista general**

Ahora mismo `Parser/` tiene dos líneas de trabajo en paralelo:

1. un parser recursivo manual para aprender y validar el subconjunto del lenguaje
2. la infraestructura inicial del generador LL(1), que será el camino principal

El flujo actual es este:

- `Lexer` produce tokens
- `Parser/core` los adapta al contrato del parser
- `Parser/syntax/parser.cpp` puede parsear expresiones manualmente
- `Parser/grammar/grammar.ll1` formaliza la sintaxis en archivo
- `Parser/generator/grammar_reader.cpp` lee esa gramática y la convierte en estructuras internas
- `Parser/generator/first_follow.cpp` calcula FIRST/FOLLOW sobre esa gramática
- `Parser/generator/ll1_table.cpp` construye la tabla predictiva y detecta conflictos
- `Parser/syntax/ll1_parser.cpp` consume tokens usando la tabla predictiva
- `Parser/ast/cst_nodes.*` modela el CST y `Ll1Parser` lo construye durante el parseo
- `Parser/ast/cst_to_ast.cpp` reduce el CST a un AST util para semantica
- `Compiler/main.cpp` conecta todo el pipeline desde codigo HULK real

---

**1. Parser/core**

[Parser/core/token.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/core/token.hpp)

Este archivo define el contrato básico del parser.

- `enum class TokenType`
  Es el vocabulario completo de tokens que el parser entiende.
  Aquí están literales, keywords, signos, operadores y tokens técnicos como `EOF_TOKEN`.

- `struct Token`
  Representa un token individual con:
  - `type`
  - `lexeme`
  - `line`
  - `col`

- `using TokenList = std::vector<Token>`
  Es la secuencia materializada de tokens que entra al parser.

- `token_name(TokenType type)`
  Convierte un `TokenType` a texto.
  Sirve para debugging, mensajes de error y pruebas.

[Parser/core/parse_error.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/core/parse_error.hpp)

Este archivo encapsula errores sintácticos.

- `class ParseError : public std::runtime_error`
  Guarda el token encontrado y construye un mensaje legible.

- `ParseError(const Token& found, const std::string& message)`
  Crea la excepción a partir del token problemático y una explicación.

- `found() const`
  Devuelve el token que disparó el error.

- `build_message(...)`
  Arma el mensaje final con línea, columna, tipo y lexema.

Ejemplo de mensaje:
`Error de parseo en linea 1, columna 4: Se esperaba ')' ...`

[Parser/core/token_stream.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/core/token_stream.hpp)  
[Parser/core/token_stream.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/core/token_stream.cpp)

Esta clase es la interfaz de navegación sobre `TokenList`.

- `TokenStream(TokenList tokens)`
  Recibe la lista de tokens.
  Si la lista viene vacía, inserta un `EOF_TOKEN` artificial para que el parser nunca quede sin centinela.

- `current() const`
  Devuelve el token actual.

- `peek(std::size_t offset = 1) const`
  Mira adelante sin consumir.

- `previous() const`
  Devuelve el último token consumido.

- `advance()`
  Avanza una posición y devuelve el token que acaba de consumir.

- `is(TokenType type) const`
  Pregunta si el token actual es de cierto tipo.

- `match(TokenType type)`
  Si coincide, consume y devuelve `true`; si no, no consume y devuelve `false`.

- `consume(TokenType type, const std::string& message)`
  Exige un token exacto.
  Si no coincide, lanza `ParseError`.

- `at_end() const`
  Dice si ya estás en `EOF_TOKEN`.

- `position() const`
  Devuelve el índice actual dentro del stream.

Esta clase es la base del parser manual y luego también puede servir en utilidades o pruebas.

[Parser/core/token_adapter.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/core/token_adapter.hpp)  
[Parser/core/token_adapter.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/core/token_adapter.cpp)

Esta capa conecta el lexer con el parser.

- `from_lexer_token(::TokenType lexer_type)`
  Traduce el enum del lexer al enum del parser.
  Ahora casi son iguales, pero esto mantiene el desacople.

- `make_token(const HulkLexer& lexer, ::TokenType lexer_type)`
  Construye un `parser::Token` usando:
  - tipo traducido
  - lexema actual
  - línea
  - columna

- `tokenize_stream(std::istream& input)`
  Ejecuta `HulkLexer`, consume todos los tokens y devuelve un `TokenList`.

- `tokenize_file(const std::string& path)`
  Abre un archivo y delega en `tokenize_stream`.

La idea aquí es importante: el parser no depende directamente de Flex ni de detalles internos del lexer.

---

**2. Parser/ast**

[Parser/ast/expr.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/ast/expr.hpp)  
[Parser/ast/expr.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/ast/expr.cpp)

Este AST existe hoy para el parser manual. Más adelante, en el diseño LL(1) final, el AST idealmente saldrá de `cst_to_ast`, no directamente del parser.

- `enum class ExprKind`
  Distingue clases de expresión:
  - `NUMBER`
  - `STRING`
  - `BOOL`
  - `IDENTIFIER`
  - `GROUPED`
  - `UNARY`
  - `BINARY`

- `struct Expr`
  Base abstracta mínima de todas las expresiones.
  Tiene `kind` y destructor virtual.

- `using ExprPtr = std::unique_ptr<Expr>`
  Alias para manejar ownership sin fugas.

Nodos concretos:

- `NumberExpr(Token token)`
  Guarda el token del número.

- `StringExpr(Token token)`
  Guarda el token del string.

- `BoolExpr(Token token, bool value)`
  Guarda token y valor booleano real.

- `IdentifierExpr(Token token)`
  Guarda el identificador.

- `GroupedExpr(Token lparen, ExprPtr expression)`
  Representa una expresión entre paréntesis.

- `UnaryExpr(Token op, ExprPtr right)`
  Representa un operador unario y su operando.

- `BinaryExpr(ExprPtr left, Token op, ExprPtr right)`
  Representa una operación binaria.

- `expr_to_string(const Expr& expr)`
  Convierte el AST a una cadena legible.
  Se usa en las smoke tests para comparar estructura esperada vs estructura obtenida.

Ejemplo:
`Binary(Number(1), +, Binary(Number(2), *, Number(3)))`

---

**3. Parser/syntax: parser manual**

[Parser/syntax/parser.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/syntax/parser.hpp)  
[Parser/syntax/parser.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/syntax/parser.cpp)

Este es el parser recursivo manual. Hoy sirve como prototipo didáctico y como referencia para validar la gramática formal.

- `Parser(TokenList tokens)`
  Recibe los tokens y crea internamente un `TokenStream`.

- `parse_expression()`
  Punto de entrada para expresiones.
  Ahora entra por `parse_or()`, que es el nivel de menor precedencia.

- `parse_expression_statement()`
  Parsea una expresión y luego exige `SEMICOLON`.
  Por eso las pruebas usan cosas como `42;` o `1 + 2;`.

Métodos privados por precedencia:

- `parse_or()`
  Parsea expresiones con `or`.
  Usa un bucle, así que `a or b or c` se agrupa a izquierda.

- `parse_and()`
  Igual para `and`.

- `parse_comparison()`
  Maneja `<`, `<=`, `>`, `>=`, `==`, `!=`.

- `parse_concat()`
  Maneja `CONCAT` y `CONCAT_WS`.

- `parse_add()`
  Maneja `+` y `-`.

- `parse_mul()`
  Maneja `*` y `/`.

- `parse_power()`
  Maneja `^`.
  Aquí está el detalle clave: usa recursión a la derecha, no bucle.
  Por eso `2 ^ 3 ^ 4` se parsea como `2 ^ (3 ^ 4)`.

- `parse_unary()`
  Maneja `-`, `!`, `~` antes de bajar a primaria.

- `parse_primary()`
  Reconoce:
  - número
  - string
  - `true`
  - `false`
  - identificador
  - paréntesis

Este parser está hardcodeado por funciones y precedencia. Justamente por eso no será el parser final del compilador, pero sí fue útil para validar el subconjunto del lenguaje.

---

**4. Parser/grammar**

[Parser/grammar/grammar.ll1](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/grammar/grammar.ll1)

Este archivo es la versión formal LL(1) del subconjunto actual.

Qué contiene:

- `Program -> ExprStmt EOF_TOKEN`
- `ExprStmt -> Expr SEMICOLON`
- descomposición por niveles:
  - `OrExpr`
  - `AndExpr`
  - `CmpExpr`
  - `ConcatExpr`
  - `AddExpr`
  - `MulExpr`
  - `PowerExpr`
  - `UnaryExpr`
  - `PostfixExpr`
  - `Primary`

Patrón importante:
- recursión izquierda eliminada
- colas tipo `Tail`
- epsilon explícito como `ε`

Ejemplo:
- `OrExpr -> AndExpr OrExprTail`
- `OrExprTail -> OR AndExpr OrExprTail | ε`

Eso es exactamente el tipo de forma que necesita un parser LL(1).

Caso especial de potencia:
- `PowerExpr -> UnaryExpr PowerExprTail`
- `PowerExprTail -> CARET PowerExpr | ε`

Eso preserva asociatividad por la derecha.

---

**5. Parser/generator**

[Parser/generator/production.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/production.hpp)

Aquí empieza la infraestructura del generador LL(1).

- `kEpsilonSymbol`
  Constante textual para `ε`.

- `struct Production`
  Representa una producción individual.
  - `lhs`: no terminal del lado izquierdo
  - `rhs`: secuencia de símbolos del lado derecho

- `is_epsilon() const`
  Devuelve `true` si `rhs` está vacío.
  Esa es la representación interna elegida para epsilon.

- `struct Grammar`
  Representa una gramática completa con:
  - `start_symbol`
  - `productions`
  - `non_terminals`
  - `terminals`

- `production_to_string(...)`
  Convierte una producción a texto.
  Sirve para debug y pruebas.

[Parser/generator/grammar_reader.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/grammar_reader.hpp)

API pública del lector.

- `read_grammar(std::istream& input)`
  Lee una gramática desde cualquier stream.

- `read_grammar_file(const std::string& path)`
  Lee desde archivo.

[Parser/generator/grammar_reader.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/grammar_reader.cpp)

Este archivo hace el trabajo real.

Funciones auxiliares internas:

- `trim(const std::string& value)`
  Quita espacios al principio y al final.

- `split_symbols(const std::string& text)`
  Parte un lado derecho como `"CARET PowerExpr"` en `["CARET", "PowerExpr"]`.

- `add_alternatives(...)`
  Toma algo como:
  `OR AndExpr OrExprTail | ε`
  y lo convierte en dos `Production`.
  Si encuentra `ε`, deja `rhs` vacío.

- `collect_terminals(Grammar& grammar)`
  Recorre todos los símbolos del `rhs`.
  Si un símbolo no está en `non_terminals`, lo considera terminal.

Funciones públicas:

- `production_to_string(...)`
  Reconstruye texto como:
  - `Program -> ExprStmt EOF_TOKEN`
  - `OrExprTail -> ε`

- `read_grammar(std::istream& input)`
  Recorre el archivo línea por línea.
  Reglas:
  - ignora vacías y comentarios
  - si encuentra `->`, empieza una nueva producción base
  - si la línea empieza con `|`, la trata como continuación del `lhs` anterior
  - el primer `lhs` se guarda como `start_symbol`
  - todos los `lhs` se meten en `non_terminals`
  - al final infiere terminales y valida que haya producciones

- `read_grammar_file(...)`
  Abre el archivo y delega en `read_grammar`.

Errores que puede lanzar:
- lado izquierdo vacío
- lado derecho vacío
- alternativa vacía
- `|` sin una producción previa
- línea con formato desconocido
- archivo no abierto
- gramática vacía

---

**6. FIRST y FOLLOW**

[Parser/generator/first_follow.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/first_follow.hpp)  
[Parser/generator/first_follow.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/first_follow.cpp)

Esta capa toma una `Grammar` ya leida y calcula la informacion teorica necesaria para construir una tabla LL(1).

- `using SymbolSet = std::set<std::string>`
  Representa un conjunto de simbolos.

- `using SymbolSetMap = std::map<std::string, SymbolSet>`
  Mapea un simbolo a su conjunto `FIRST` o `FOLLOW`.

- `struct FirstFollowResult`
  Agrupa:
  - `first_sets`
  - `follow_sets`

- `compute_first_follow(const Grammar& grammar)`
  Calcula todos los `FIRST` y todos los `FOLLOW`.

- `compute_first_of_sequence(const std::vector<std::string>& sequence, ...)`
  Calcula `FIRST(alpha)` para una secuencia arbitraria de simbolos.

Como funciona internamente:

1. inicializa `FIRST(terminal) = { terminal }`
2. inicializa `FIRST(no_terminal) = {}`
3. inicializa `FOLLOW(start_symbol) = { EOF_TOKEN }`
4. itera hasta punto fijo para propagar `FIRST`
5. itera hasta punto fijo para propagar `FOLLOW`

Detalles importantes:

- epsilon se representa como el string `ε`
- una produccion epsilon internamente es `rhs` vacio
- si todos los simbolos de una secuencia pueden derivar epsilon, entonces `FIRST(secuencia)` contiene `ε`
- cuando el sufijo de un no terminal puede derivar epsilon, se propaga `FOLLOW(lhs)` hacia ese no terminal

El archivo tambien incluye helpers internos:

- `is_non_terminal(...)`
- `is_terminal(...)`
- `insert_all_except_epsilon(...)`
- inicializacion de conjuntos base

Con esto queda lista la base matematica para la siguiente fase:

- construir la tabla LL(1)

---

**7. Cómo funciona `first_follow_smoke.cpp`**

[Parser/tests/first_follow_smoke.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/first_follow_smoke.cpp)

Esta prueba:

1. lee `grammar.ll1`
2. calcula `FIRST` y `FOLLOW`
3. verifica que algunos conjuntos importantes contengan los simbolos esperados

Checks principales:

- `FIRST(Primary)` contiene todos los inicios validos de una primaria
- `FIRST(UnaryExpr)` contiene operadores unarios y tambien los inicios de `Primary`
- `FIRST(PostfixTail)` contiene `LPAREN`, `DOT` y epsilon
- `FIRST(OrExprTail)` contiene epsilon
- `FOLLOW(Expr)` contiene `SEMICOLON`, `RPAREN` y `EOF_TOKEN`
- `FOLLOW(Primary)` contiene operadores y cierres que pueden venir despues de una primaria
- `FIRST(UnaryExpr PowerExprTail)` funciona correctamente para una secuencia, no solo para un simbolo aislado

La prueba no verifica aun:

- que la tabla LL(1) no tenga conflictos
- que la gramatica completa sea LL(1) de punta a punta
- que el parser predictivo ya exista

Eso queda para la siguiente fase.

---

**8. Tabla LL(1)**

[Parser/generator/ll1_table.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/ll1_table.hpp)  
[Parser/generator/ll1_table.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/generator/ll1_table.cpp)

Esta capa toma:

- una `Grammar`
- sus conjuntos `FIRST`
- sus conjuntos `FOLLOW`

y produce la tabla predictiva que usara el parser LL(1).

Piezas principales:

- `Ll1Table`
  Mapa doble:
  - fila: no terminal
  - columna: terminal lookahead
  - valor: produccion elegida

- `Ll1Conflict`
  Registra cuando dos producciones distintas intentan ocupar la misma celda `M[A, a]`.

- `Ll1TableResult`
  Agrupa:
  - `table`
  - `conflicts`

- `build_ll1_table(...)`
  Construye toda la tabla usando las reglas formales de LL(1).

- `conflict_to_string(...)`
  Convierte un conflicto en texto legible para debug o futuros errores.

Como funciona `build_ll1_table(...)`:

1. para cada produccion `A -> alpha`, calcula `FIRST(alpha)`
2. por cada terminal `a` en `FIRST(alpha)` distinto de epsilon, inserta `A -> alpha` en `M[A, a]`
3. si `alpha` puede derivar epsilon, entonces para cada `b` en `FOLLOW(A)` inserta `A -> alpha` en `M[A, b]`
4. si una celda ya estaba ocupada por otra produccion distinta, registra un `Ll1Conflict`

Con esto ya se puede responder la pregunta central de LL(1):

- dado un no terminal actual y un token lookahead, que produccion debo expandir

---

**9. Cómo funciona `ll1_table_smoke.cpp`**

[Parser/tests/ll1_table_smoke.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/ll1_table_smoke.cpp)

Esta prueba:

1. lee `grammar.ll1`
2. calcula `FIRST/FOLLOW`
3. construye la tabla LL(1)
4. verifica que no haya conflictos
5. revisa varias celdas concretas de la tabla

Checks principales:

- `M[Primary, NUMBER_LITERAL] = Primary -> NUMBER_LITERAL`
- `M[Primary, LPAREN] = Primary -> LPAREN Expr RPAREN`
- `M[OrExprTail, OR] = OrExprTail -> OR AndExpr OrExprTail`
- `M[OrExprTail, SEMICOLON] = OrExprTail -> ε`
- `M[PowerExprTail, CARET] = PowerExprTail -> CARET PowerExpr`
- `M[PowerExprTail, PLUS] = PowerExprTail -> ε`
- `M[PostfixTail, LPAREN] = PostfixTail -> LPAREN ArgListOpt RPAREN PostfixTail`
- `M[PostfixTail, DOT] = PostfixTail -> DOT IDENTIFIER PostfixTail`

Eso no solo comprueba que la tabla existe.

Comprueba tambien que:

- se insertaron bien producciones normales
- se insertaron bien producciones epsilon
- la gramatica actual no genera conflictos LL(1)

---

**10. Parser LL(1)**

[Parser/syntax/ll1_parser.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/syntax/ll1_parser.hpp)  
[Parser/syntax/ll1_parser.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/syntax/ll1_parser.cpp)

Esta capa usa:

- `TokenList`
- `Grammar`
- tabla LL(1)

para hacer parseo predictivo generico sin hardcodear producciones del lenguaje.

Piezas principales:

- `Ll1ParseResult`
  Guarda una traza simple de la derivacion, es decir, las producciones aplicadas.

- `Ll1Parser`
  Implementa el algoritmo de pila LL(1).

Metodos importantes:

- `parse()`
  Ejecuta el ciclo principal:
  1. inicializa una pila con el simbolo inicial
  2. mira el simbolo de arriba
  3. si es terminal, intenta consumirlo
  4. si es no terminal, consulta la tabla y expande
  5. repite hasta vaciar la pila

- `current_terminal()`
  Convierte el token actual a su nombre terminal usando `token_name(...)`.

- `is_terminal(...)`
  Revisa si un simbolo pertenece a `grammar.terminals`.

- `is_non_terminal(...)`
  Revisa si un simbolo pertenece a `grammar.non_terminals`.

- `match_terminal(...)`
  Verifica que el lookahead actual coincida con el terminal esperado.

- `select_production(...)`
  Busca la produccion en `M[A, a]`.
  Si no existe, lanza `ParseError`.

Detalle importante:

- `EOF_TOKEN` se trata como terminal logico de cierre.
  El parser lo valida, pero no intenta avanzar despues de el.

En la iteracion 1 de postfix, este parser ya valida casos reales como:

- `a.b(c);`
- rechazo de `a.;`

Desde la fase 9, ademas de reconocer con la tabla:

- valida entrada
- registra la derivacion aplicada
- construye el CST mientras expande producciones y consume terminales

---

**11. Cómo funciona `ll1_parser_smoke.cpp`**

[Parser/tests/ll1_parser_smoke.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/ll1_parser_smoke.cpp)

Esta prueba:

1. lee `grammar.ll1`
2. calcula `FIRST/FOLLOW`
3. construye la tabla LL(1)
4. instancia `Ll1Parser`
5. prueba un caso valido y uno invalido

Checks principales:

- `1 + 2 * 3;` debe aceptarse
- `1 + ;` debe rechazarse con `ParseError`

**Flujo real del caso valido**

Tomemos esta entrada de prueba:

```text
1 + 2 * 3 ;
```

El `TokenList` equivalente es, conceptualmente:

```text
NUMBER_LITERAL PLUS NUMBER_LITERAL STAR NUMBER_LITERAL SEMICOLON EOF_TOKEN
```

El parser LL(1) no mira la cadena completa de una vez.

Trabaja con dos cosas al mismo tiempo:

1. una pila de simbolos gramaticales
2. un lookahead, que es el token actual del `TokenStream`

La pila empieza con:

```text
[ Program ]
```

Y el lookahead inicial es:

```text
NUMBER_LITERAL
```

Entonces el flujo es este:

1. el tope de la pila es `Program`
   No es terminal, asi que el parser consulta la tabla en `M[Program, NUMBER_LITERAL]`
   La tabla devuelve:
   `Program -> ExprStmt EOF_TOKEN`

2. el parser saca `Program` de la pila y empuja el lado derecho al reves:

```text
[ EOF_TOKEN, ExprStmt ]
```

3. el tope ahora es `ExprStmt`
   Consulta `M[ExprStmt, NUMBER_LITERAL]`
   Obtiene:
   `ExprStmt -> Expr SEMICOLON`

4. empuja al reves:

```text
[ EOF_TOKEN, SEMICOLON, Expr ]
```

5. el tope es `Expr`
   Consulta `M[Expr, NUMBER_LITERAL]`
   Obtiene:
   `Expr -> OrExpr`

6. a partir de ahi pasa por la jerarquia de expresiones:
   `OrExpr -> AndExpr OrExprTail`
   `AndExpr -> CmpExpr AndExprTail`
   `CmpExpr -> ConcatExpr CmpExprTail`
   `ConcatExpr -> AddExpr ConcatExprTail`
   `AddExpr -> MulExpr AddExprTail`
   `MulExpr -> PowerExpr MulExprTail`
   `PowerExpr -> UnaryExpr PowerExprTail`
   `UnaryExpr -> Primary`
   `Primary -> NUMBER_LITERAL`

7. cuando el tope de la pila finalmente es el terminal `NUMBER_LITERAL`, el parser ya no expande nada:
   compara el terminal esperado con el lookahead actual
   como coinciden, consume el token `1`

8. el nuevo lookahead pasa a ser `PLUS`
   el parser sigue con los simbolos que quedaron en la pila, en particular los tails:
   - `PowerExprTail`
   - `MulExprTail`
   - `AddExprTail`
   etc.

9. ahora la tabla decide si cada tail continua o se va por epsilon:
   - `M[PowerExprTail, PLUS] = PowerExprTail -> ε`
   - `M[MulExprTail, PLUS] = MulExprTail -> ε`
   - `M[AddExprTail, PLUS] = AddExprTail -> PLUS MulExpr AddExprTail`

   Eso significa:
   - la potencia actual termino
   - la multiplicacion actual termino
   - pero la suma debe continuar

10. como `AddExprTail` eligio la produccion con `PLUS`, el parser termina esperando el terminal `PLUS`, lo consume y sigue parseando el `MulExpr` de la derecha

11. el mismo mecanismo se repite con `2 * 3`
   Cuando el lookahead es `STAR`, la tabla elige:
   `M[MulExprTail, STAR] = MulExprTail -> STAR PowerExpr MulExprTail`

   Eso es exactamente lo que hace que `*` tenga mas precedencia que `+`:
   el `MulExpr` del lado derecho de la suma sigue creciendo antes de que `AddExprTail` cierre

12. cuando el lookahead pasa a `SEMICOLON`, la mayoria de tails se van por epsilon:
   - `MulExprTail -> ε`
   - `AddExprTail -> ε`
   - `ConcatExprTail -> ε`
   - `CmpExprTail -> ε`
   - `AndExprTail -> ε`
   - `OrExprTail -> ε`

13. al terminar la expresion, la pila todavia contiene `SEMICOLON` y luego `EOF_TOKEN`
   El parser consume `SEMICOLON`
   Luego valida `EOF_TOKEN`

14. cuando la pila queda vacia y no quedan tokens reales por consumir, el parseo termina con exito

**Como decide el parser que la expresion es valida**

La entrada es valida si durante todo el proceso pasan estas dos condiciones:

1. para cada no terminal `A` y lookahead `a`, existe una entrada valida en la tabla `M[A, a]`
2. para cada terminal esperado, el token actual coincide exactamente

Si ambas cosas se cumplen hasta vaciar la pila, entonces la secuencia de tokens pertenece a la gramatica.

Dicho de otra forma:

- la tabla decide que produccion aplicar
- el `TokenStream` decide si los terminales reales coinciden con lo esperado
- la pila asegura que el orden de derivacion se respete

**Como detecta que `1 + ;` es invalida**

En el caso invalido:

```text
NUMBER_LITERAL PLUS SEMICOLON EOF_TOKEN
```

el parser logra consumir:

- `NUMBER_LITERAL`
- `PLUS`

pero luego necesita comenzar el `MulExpr` que viene despues del `PLUS`.

Eso obliga a bajar hasta algo como:

- `MulExpr`
- `PowerExpr`
- `UnaryExpr`
- `Primary`

El problema es que el lookahead en ese momento es `SEMICOLON`.

La tabla no tiene una entrada valida como:

```text
M[Primary, SEMICOLON]
```

porque una primaria no puede empezar con `;`.

Entonces `select_production(...)` falla y lanza `ParseError`.

Esa es exactamente la idea del parser LL(1):

- si el lookahead no pertenece al conjunto de terminales validos para expandir un no terminal dado
- entonces la entrada no puede derivarse desde la gramatica

**Qué demuestra realmente esta prueba**

Esta smoke test demuestra que ya funciona el ciclo completo:

- tokens
- gramática
- FIRST/FOLLOW
- tabla LL(1)
- parser predictivo

Y demuestra, además, algo importante sobre precedencia:

- la validez de `1 + 2 * 3;` no depende de funciones hardcodeadas como `parse_add()` o `parse_mul()`
- depende de que la gramatica LL(1) y la tabla resultante codifican correctamente la jerarquia de expresiones

Eso comprueba que el parser LL(1) ya no solo tiene datos teoricos.

Ya puede:

- consultar la tabla
- decidir expansiones
- consumir tokens
- fallar correctamente cuando el lookahead no permite ninguna produccion

---

**12. Nodos de CST**

[Parser/ast/cst_nodes.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/ast/cst_nodes.hpp)  
[Parser/ast/cst_nodes.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/ast/cst_nodes.cpp)

Esta capa modela el arbol de derivacion que produce el parser LL(1).

Piezas principales:

- `CstNode`
  Representa un simbolo del arbol.
  Guarda:
  - `symbol`
  - `has_token`
  - `token`
  - `children`

- `add_child(...)`
  Agrega un hijo al nodo actual y devuelve un puntero crudo estable para seguir construyendo el arbol.

- `cst_to_string(...)`
  Imprime el arbol de forma jerarquica para debug.

Detalle importante:

- los nodos terminales pueden guardar el token real que se consumio
- las producciones epsilon crean un hijo explicito con simbolo `ε`

Eso hace que el CST refleje la derivacion de forma literal.

---

**13. Cómo se integra el CST en `Ll1Parser`**

Ahora la pila del parser LL(1) ya no guarda solo simbolos.

Guarda pares:

- simbolo gramatical
- nodo del CST asociado a ese simbolo

Cuando el parser expande un no terminal:

1. selecciona la produccion desde la tabla
2. crea nodos hijos en el CST para cada simbolo del lado derecho
3. empuja esos hijos a la pila en orden inverso

Cuando el parser consume un terminal:

1. verifica que el lookahead coincida
2. marca el nodo del CST con `has_token = true`
3. copia el token consumido en ese nodo
4. avanza el `TokenStream`

Cuando la produccion es epsilon:

1. no empuja nuevos simbolos a la pila
2. crea un hijo explicito `ε` bajo el no terminal actual

De ese modo, la misma ejecucion del parser:

- reconoce la entrada
- registra la derivacion
- arma el arbol completo

---

**14. Cómo funciona `ll1_cst_smoke.cpp`**

[Parser/tests/ll1_cst_smoke.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/ll1_cst_smoke.cpp)

Esta prueba:

1. lee `grammar.ll1`
2. calcula `FIRST/FOLLOW`
3. construye la tabla LL(1)
4. ejecuta el parser LL(1) con construccion de CST
5. valida propiedades basicas del arbol

Checks principales:

- existe `cst_root`
- la raiz se llama `Program`
- el arbol contiene `ExprStmt`
- el arbol contiene un nodo `NUMBER_LITERAL`
- ese nodo terminal guarda el token consumido
- el lexema almacenado coincide con el original
- el caso invalido `1 + ;` sigue rechazandose

Esto ya demuestra que el parser generado no solo decide producciones correctamente.

Tambien deja una estructura de arbol util para la futura fase de conversion:

- `CST -> AST`

---

**15. Conversión `CST -> AST`**

[Parser/ast/cst_to_ast.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/ast/cst_to_ast.hpp)  
[Parser/ast/cst_to_ast.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/ast/cst_to_ast.cpp)

Esta capa toma el arbol de derivacion literal y lo reduce a la estructura semantica de expresiones.

La idea central es:

- el CST conserva exactamente la forma de la gramatica
- el AST elimina nodos de ayuda como `Expr`, `OrExprTail`, `MulExprTail`, `SEMICOLON`, `EOF_TOKEN`

Piezas importantes:

- `cst_to_ast(...)`
  Punto de entrada. Espera una raiz `Program` y devuelve el AST de la expresion principal.

- helpers `build_*`
  Hay una funcion por no terminal relevante del subconjunto actual:
  - `build_expr`
  - `build_or_expr`
  - `build_and_expr`
  - `build_cmp_expr`
  - `build_concat_expr`
  - `build_add_expr`
  - `build_mul_expr`
  - `build_power_expr`
  - `build_unary_expr`
  - `build_primary`

- helpers `build_*_tail`
  Recorren los nodos `Tail` del CST y reconstruyen las operaciones binarias acumuladas.

Detalles importantes:

- los nodos epsilon del CST no aparecen en el AST final
- los nodos terminales aportan los `Token` reales para construir los nodos del AST
- `PowerExprTail` conserva asociatividad derecha porque reconstruye el lado derecho como `PowerExpr` completo
- `Primary -> LPAREN Expr RPAREN` se convierte en `GroupedExpr`
- `PostfixTail -> LPAREN ArgListOpt RPAREN ...` se convierte en `CallExpr`
- `PostfixTail -> DOT IDENTIFIER ...` se convierte en `GetAttrExpr`
- una cadena como `a.b(c)` se reconstruye como `Call(GetAttr(Identifier(a), b), [Identifier(c)])`

En esta fase la conversion cubre el subconjunto anterior y tambien llamadas/postfix de la iteracion 1.

---

**16. Cómo funciona `cst_to_ast_smoke.cpp`**

[Parser/tests/cst_to_ast_smoke.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/cst_to_ast_smoke.cpp)

Esta prueba:

1. lee la gramatica
2. calcula `FIRST/FOLLOW`
3. construye la tabla LL(1)
4. ejecuta el parser LL(1) para obtener el CST
5. convierte el CST al AST
6. compara el AST final con la forma esperada usando `expr_to_string(...)`

Checks principales:

- `1 + 2 * 3;` debe producir:
  `Binary(Number(1), +, Binary(Number(2), *, Number(3)))`

- `(2 + 3) ^ 4;` debe producir:
  `Binary(Grouped(Binary(Number(2), +, Number(3))), ^, Number(4))`

- `f(1,2);` debe producir:
  `Call(Identifier(f), [Number(1), Number(2)])`

- `a.b(c);` debe producir:
  `Call(GetAttr(Identifier(a), b), [Identifier(c)])`

Eso demuestra que la conversion ya:

- elimina ruido sintactico del CST
- conserva precedencia
- conserva agrupacion
- conserva asociatividad correcta de la potencia

---

**17. Flujo de extremo a extremo**

[Compiler/main.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Compiler/main.cpp)

Este archivo es el primer punto de entrada real que une todas las piezas del proyecto.

El flujo que ejecuta es:

1. lee `Parser/grammar/grammar.ll1`
2. calcula `FIRST/FOLLOW`
3. construye la tabla LL(1)
4. valida que no existan conflictos LL(1)
5. tokeniza el codigo HULK de entrada con `tokenize_file(...)` o `tokenize_stream(...)`
6. ejecuta `Ll1Parser`
7. si el parseo fue exitoso, opcionalmente imprime:
   - tokens
   - CST
   - AST

Opciones disponibles:

- `--tokens`
- `--cst`
- `--ast`

Si no se pasa archivo, el driver lee desde `stdin`.

Esto no significa que ya soporte todo HULK.

Significa que ya existe un flujo real:

- codigo fuente HULK
- lexer
- parser LL(1)
- CST
- AST

para el subconjunto cubierto por la gramatica actual.

Hay un ejemplo minimo compatible en:

[Parser/tests/valid_expr_pipeline.hulk](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/valid_expr_pipeline.hulk)

---

**6. Cómo funciona `grammar_reader_smoke.cpp`**

[Parser/tests/grammar_reader_smoke.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/tests/grammar_reader_smoke.cpp)

Esta prueba verifica que el lector de gramática no solo compile, sino que interprete correctamente `grammar.ll1`.

**Estructura general**

1. Define helpers pequeños
2. Llama a `read_grammar_file("Parser/grammar/grammar.ll1")`
3. Hace varias comprobaciones
4. Si todas pasan, retorna `0`
5. Si falla alguna, retorna `1`
6. Si el lector lanza excepción, la reporta como fallo

**Helper 1: `expect(...)`**

```cpp
bool expect(bool condition, const std::string& message)
```

Qué hace:
- si `condition` es `false`, imprime `[FAIL] message` y devuelve `false`
- si `condition` es `true`, imprime `[OK] message` y devuelve `true`

Por qué sirve:
- simplifica cada aserción
- te deja ver exactamente qué parte falló
- evita usar un framework de testing todavía

**Helper 2: `contains_production(...)`**

```cpp
bool contains_production(const Grammar& grammar, const std::string& expected)
```

Qué hace:
- recorre `grammar.productions`
- convierte cada `Production` a string con `production_to_string(...)`
- compara con el string esperado

Por qué sirve:
- la prueba no necesita conocer índices ni orden interno exacto
- solo verifica que cierta producción exista en la gramática cargada

**`main()` paso por paso**

```cpp
const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
```

Esto carga el archivo y devuelve un `Grammar`.

Luego:

```cpp
bool ok = true;
```

Usa un acumulador para no parar en el primer fallo.

Checks:

```cpp
ok &= expect(grammar.start_symbol == "Program", "start symbol is Program");
```
Verifica que el primer no terminal leído fue `Program`.

```cpp
ok &= expect(!grammar.productions.empty(), "grammar has productions");
```
Comprueba que el archivo produjo al menos una producción.

```cpp
ok &= expect(grammar.non_terminals.count("Expr") == 1, "Expr recognized as non-terminal");
ok &= expect(grammar.non_terminals.count("Primary") == 1, "Primary recognized as non-terminal");
```
Comprueba que símbolos del lado izquierdo fueron correctamente clasificados como no terminales.

```cpp
ok &= expect(grammar.terminals.count("NUMBER_LITERAL") == 1, "NUMBER_LITERAL recognized as terminal");
ok &= expect(grammar.terminals.count("CARET") == 1, "CARET recognized as terminal");
```
Comprueba que símbolos del lado derecho que no son `lhs` fueron clasificados como terminales.

Luego verifica producciones concretas:

```cpp
ok &= expect(contains_production(grammar, "Program -> ExprStmt EOF_TOKEN"),
             "Program production loaded");
```
Valida una producción base de entrada.

```cpp
ok &= expect(contains_production(grammar, "PowerExprTail -> CARET PowerExpr"),
             "right-associative power production loaded");
```
Esto es importante porque verifica justo la producción que asegura la asociatividad correcta de `^`.

```cpp
ok &= expect(contains_production(grammar, "OrExprTail -> ε"),
             "epsilon alternative loaded");
```
Esto comprueba que el lector interpreta bien epsilon como producción válida.

```cpp
ok &= expect(contains_production(grammar, "Primary -> LPAREN Expr RPAREN"),
             "grouped primary production loaded");
```
Esto confirma que una producción con varios símbolos se cargó bien.

Al final:

```cpp
return ok ? 0 : 1;
```

Si todas las condiciones fueron verdaderas, devuelve éxito.

Bloque `catch`:

```cpp
} catch (const std::exception& error) {
    std::cerr << "[FAIL] grammar reader threw exception: " << error.what() << "\n";
    return 1;
}
```

Si el lector falla por formato o archivo, la prueba no crashea sin contexto; imprime el mensaje del error y marca fallo.

---

**Qué demuestra realmente esta prueba**

Demuestra cuatro cosas importantes:

- que `grammar.ll1` se puede abrir y parsear
- que el lector entiende producciones normales y alternativas con `|`
- que epsilon se representa correctamente
- que ya tienes una estructura `Grammar` coherente para arrancar `FIRST/FOLLOW`

No demuestra todavía:
- que la gramática sea LL(1)
- que `FIRST/FOLLOW` estén bien
- que la tabla LL(1) no tenga conflictos
- que el parser predictivo funcione

Eso queda para las siguientes fases.

---

**Cómo ejecutarla tú mismo**

```bash
g++ -std=c++17 Parser/generator/grammar_reader.cpp Parser/tests/grammar_reader_smoke.cpp -o /tmp/grammar_reader_smoke
/tmp/grammar_reader_smoke
```

Salida esperada:

```text
[OK] start symbol is Program
[OK] grammar has productions
[OK] Expr recognized as non-terminal
[OK] Primary recognized as non-terminal
[OK] NUMBER_LITERAL recognized as terminal
[OK] CARET recognized as terminal
[OK] Program production loaded
[OK] right-associative power production loaded
[OK] epsilon alternative loaded
[OK] grouped primary production loaded
```

Si quieres, el siguiente paso puedo hacerlo de dos formas:

1. explicarte también `parser_phase2_smoke.cpp` y `parser_phase3_smoke.cpp` con este mismo nivel de detalle
2. pasar ya a Fase 6 e implementar `FIRST/FOLLOW` sobre `Grammar`
