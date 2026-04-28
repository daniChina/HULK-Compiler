Plan incremental (siguiendo regla 1..6 de plan_parser.md)
Te propongo hacerlo en iteraciones chicas, y en cada una aplicar estrictamente:

actualizar grammar.ll1
recalcular FIRST/FOLLOW
reconstruir tabla LL(1)
verificar parseo predictivo
verificar CST
verificar conversion a AST

Estado actual:
- Iteracion 1 en progreso.
- Paso 1 (actualizar `grammar.ll1`) completado.
- Paso 2 (recalcular `FIRST/FOLLOW`) completado con smoke test.
- Paso 3 (reconstruir tabla LL(1)) validado sin conflictos.
- Gramatica de postfix cerrada en forma LL(1): `DOT IDENTIFIER` y `LPAREN ... RPAREN` como sufijos encadenables.
- Pasos 4, 5 y 6 pendientes en esta iteracion.


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
Objetivo: function ...

Añadir producción de declaración de función (params, retorno, body/arrow).
Verificar FIRST/FOLLOW de FUNCTION vs expresiones.
CST de firma y cuerpo.
AST: FunctionDecl.


Iteración 9 — type, herencia, atributos, métodos, self, base, new (tu punto 9)
Objetivo: cierre del frente OO

Añadir TypeDecl, InheritsOpt, miembros.
Integrar self, base(...), new T(...) en expresiones/postfix.
Verificar LL(1) en ramas de tipo vs función vs expr.
CST detallado de tipo y miembros.
AST: TypeDecl, MethodDecl, NewExpr, BaseCallExpr, etc.