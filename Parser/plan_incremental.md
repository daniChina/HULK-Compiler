Plan incremental (siguiendo regla 1..6 de plan_parser.md)
Te propongo hacerlo en iteraciones chicas, y en cada una aplicar estrictamente:

actualizar grammar.ll1
recalcular FIRST/FOLLOW
reconstruir tabla LL(1)
verificar parseo predictivo
verificar CST
verificar conversion a AST

Estado actual:
- Iteracion 1 completada.
- Paso 1 (actualizar `grammar.ll1`) completado.
- Paso 2 (recalcular `FIRST/FOLLOW`) completado con smoke test.
- Paso 3 (reconstruir tabla LL(1)) validado sin conflictos.
- Gramatica de postfix cerrada en forma LL(1): `DOT IDENTIFIER` y `LPAREN ... RPAREN` como sufijos encadenables.
- Paso 4 (verificar parseo predictivo) completado con casos postfix validos e invalidos.
- Paso 5 (verificar CST) completado con chequeos de `PostfixExpr`, `DOT` y `LPAREN`.
- Paso 6 (verificar conversion a AST) completado con `CallExpr` y `GetAttrExpr`.
- Iteración 2 (LetExpr) completada: gramática LL(1) extendida, AST adaptado con nodos LetExpr anidados y cst_to_ast preparado.
- Iteración 3 (Bloques) completada: gramática con BlockExpr (LBRACE BlockList RBRACE), nodo BlockExpr incorporado al AST y conversión desde CST.
- Iteración 4 (If/Elif/Else) completada: gramática para control de flujo condicional con y sin else, normalización de elif anidados.
- Iteración 5 (While) completada: gramática de ciclos condicionales iterativos y su mapeo al nodo WhileExpr en el AST.
- Iteración 6 (For) completada: gramática de ciclos iterativos por colección, mapeado al nodo ForExpr en el AST.
- Iteración 7 (Programa/Stmt) completada: nodo raíz elevado a lista de Stmt, integrando ExprStmt y preparando terreno para declaraciones.

Iteración 1 — llamadas y postfix (tu punto 1)
Objetivo: f(), f(a,b), a.b, a.b(), a.b(c).d

Gramática: introducir PostfixExpr + PostfixTail* y ArgList.
FIRST/FOLLOW + tabla: asegurar que PostfixTail sea LL(1) con DOT / LPAREN / ε.
Parseo: casos válidos e inválidos (f(,), a., a..b).
CST: validar cadena de sufijos (encadenamiento).
AST: agregar/usar nodos tipo CallExpr, GetAttrExpr, MethodCallExpr (o equivalentes tuyos).


Iteración 2 — let (tu punto 2)
Objetivo: let x = 1 in x+1, múltiples bindings

Añadir no terminales LetExpr, BindingList, BindingTail.
Verificar LL(1) entre let y resto de Expr.
CST de bindings y in.
AST: desugar a nested-let o nodo explícito LetExpr.


Iteración 3 — bloques (tu punto 3)
Objetivo: { expr; expr; ... }

Añadir BlockExpr y lista de sentencias/expresiones.
Revisar separación con Program/ExprStmt.
CST con {} y secuencia interna.
AST: BlockExpr (o ExprBlock) con lista ordenada.


Iteración 4 — if / elif / else (tu punto 4)
Objetivo: ramas encadenadas con y sin else

Añadir IfExpr, ElifChainOpt, ElseOpt.
Verificar no ambigüedad de ELIF/ELSE.
CST: forma completa de cadena.
AST: normalizar elif a if anidado.


Iteración 5 — while (tu punto 5)
Objetivo: while (...) expr|block

Añadir WhileExpr.
Verificar FIRST/FOLLOW con if, for, let, etc.
CST con condición + cuerpo.
AST: WhileExpr(cond, body).


Iteración 6 — for (tu punto 6)
Objetivo: for (...) ...

Añadir ForExpr y su forma exacta HULK.
Tabla LL(1) estable con tokens FOR, IN.
CST completo del header.
AST: ForExpr(var, iterable, body).


Iteración 7 — programa completo y lista de sentencias (tu punto 7)
Objetivo: dejar de depender de “solo ExprStmt EOF”

Elevar Program a lista de Stmt.
Integrar ExprStmt, DeclStmt y separadores SEMICOLON.
CST raíz con múltiples hijos.
AST raíz Program real.


Iteración 8 — funciones (tu punto 8)
Objetivo: Soportar la declaración de funciones globales (tanto en formato _arrow_ como en bloque).
Ejemplo: `function f(x: Number): Number => x + 1;` o `function f(x) { ... }`

**Pasos detallados de implementación:**
1. **Gramática (`grammar.ll1`)**: 
   - Añadir `FunctionDecl` como una nueva opción en `Stmt`.
   - Definir la estructura base: `FunctionDecl -> FUNCTION IDENTIFIER LPAREN ArgIdListOpt RPAREN TypeAnnotationOpt FunctionBody`.
   - Implementar las sub-producciones de argumentos (`ArgIdListOpt`, `ArgIdList`, `ArgIdListTail`), que manejen el identificador y su tipo opcional.
   - Definir `TypeAnnotationOpt -> COLON IDENTIFIER | ε`.
   - Diferenciar el cuerpo: `FunctionBody -> ARROW Expr SEMICOLON | BlockExpr`.
2. **AST (`expr.hpp` y `expr.cpp`)**:
   - Añadir `StmtKind::FUNCTION_DECL`.
   - Diseñar el `struct FunctionDecl : Stmt` incluyendo: nombre (Token), parámetros (vector de pares: Token y tipo), tipo de retorno (Token opcional), y el cuerpo (`ExprPtr`).
   - Escribir su función de _stringification_.
3. **CST a AST (`cst_to_ast.cpp`)**:
   - Añadir la rama condicional en `build_stmt` para derivar hacia `FunctionDecl`.
   - Codificar `build_function_decl` asegurando extraer recursivamente los parámetros (recorriendo `ArgIdListOpt`) y diferenciando el tipo de cuerpo según la derivación.
4. **Pruebas y Documentación**:
   - Compilar el proyecto para verificar que no haya colisiones en la tabla LL(1) entre la palabra reservada `FUNCTION` y las expresiones.
   - Actualizar `README.md` y `flujo_parser.md` reflejando los aprendizajes y cambios.


Iteración 9 — type, herencia, atributos, métodos, self, base, new (tu punto 9)
Objetivo: cierre del frente OO

Añadir TypeDecl, InheritsOpt, miembros.
Integrar self, base(...), new T(...) en expresiones/postfix.
Verificar LL(1) en ramas de tipo vs función vs expr.
CST detallado de tipo y miembros.
AST: TypeDecl, MethodDecl, NewExpr, BaseCallExpr, etc.