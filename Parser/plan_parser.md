La meta principal ya no debe ser seguir creciendo el parser recursivo manual como si fuera el parser final del compilador.

En este punto del proyecto, la direccion correcta es:

1. mantener el parser recursivo actual solo como prototipo de aprendizaje y validacion
2. mover el trabajo principal al pipeline de un generador LL(1)
3. usar `grammar.ll1` como fuente formal de la sintaxis
4. construir primero CST
5. dejar la transformacion a AST como paso posterior y separado

Ese flujo es el que mas se parece al repo de amabe cuando usa:

- `grammar.ll1`
- `ll1_parser.*`
- `cst_to_ast.*`

---

# Estado actual

## Ya implementado

### Fase 1. Infraestructura minima

Ya existe en `Parser/core/`:

- `token.hpp`
- `parse_error.hpp`
- `token_stream.hpp`
- `token_stream.cpp`
- `token_adapter.hpp`
- `token_adapter.cpp`

Eso ya resuelve:

- contrato `TokenList`
- conversion lexer -> parser
- `TokenStream`
- error sintactico basico

### Fase 2. Primarias

Ya existe un parser recursivo pequeno que reconoce:

- numeros
- strings
- `true`
- `false`
- identificadores
- parentesis

### Fase 3. Precedencia

Ya existe un parser recursivo de expresiones con:

- `or`
- `and`
- comparacion
- concatenacion
- suma y resta
- multiplicacion y division
- potencia
- unarios

Y `^` ya quedo con asociatividad correcta por la derecha.

## Como debe interpretarse lo actual

Lo actual no es trabajo perdido.

Sirve para:

- entender el lenguaje
- validar el contrato de tokens
- comprobar precedencia y asociatividad
- tener casos de prueba pequenos

Pero no debe ser el parser final si el objetivo del proyecto es hacer un generador LL(1).

---

# Relacion correcta entre las piezas

La arquitectura objetivo del compilador debe ser esta:

## 1. `TokenList`

Sale del lexer y entra al parser.

El lexer produce tokens materializados con:

- tipo
- lexema
- linea
- columna

## 2. Gramatica LL(1)

La sintaxis formal del lenguaje debe vivir en un archivo como:

- `Parser/grammar/grammar.ll1`

Esa gramatica debe estar:

- sin recursion izquierda
- factorizada cuando haga falta
- preparada para construir tabla LL(1)

Ejemplo:

```text
Power      -> Unary PowerTail
PowerTail  -> CARET Power | ε
```

o cualquier forma equivalente que conserve asociatividad correcta y sea apta para LL(1).

## 3. Generador LL(1)

Debe leer la gramatica y construir:

- producciones
- conjunto de no terminales
- conjunto de terminales
- FIRST
- FOLLOW
- tabla LL(1)

## 4. Parser predictivo generado

Debe usar la tabla LL(1) para:

- expandir no terminales
- consumir tokens
- detectar errores sintacticos
- construir CST

## 5. CST o arbol de derivacion

El CST refleja literalmente la gramatica.

Eso lo hace la mejor estructura intermedia para un parser generado, porque:

- conserva cada produccion aplicada
- permite debuggear derivaciones
- desacopla reconocimiento de construccion semantica

## 6. AST

El AST no reemplaza la gramatica.

El AST se construye despues, a partir del CST, para:

- eliminar ruido sintactico
- compactar la estructura
- preparar analisis semantico
- preparar interpretacion o compilacion

La relacion correcta es:

- la gramatica dice como reconocer
- el CST conserva la forma exacta reconocida
- el AST conserva solo la estructura util

---

# Nuevo plan de trabajo

La prioridad ahora debe cambiar desde:

- "seguir agregando funciones manuales en `parser.cpp`"

hacia:

- "empezar la infraestructura del generador LL(1)"

---

# Fase 4. Formalizar la gramatica LL(1)

## Objetivo

Crear la fuente formal de la sintaxis en un archivo `.ll1`.

## Que hacer

1. crear `Parser/grammar/grammar.ll1`
2. llevar al archivo el subconjunto ya validado:
   - expresiones primarias
   - precedencia de operadores
3. escribir las producciones en forma LL(1):
   - sin recursion izquierda
   - con factorizacion cuando haga falta
4. separar claramente:
   - terminales
   - no terminales
   - epsilon

## Resultado esperado

Una primera gramatica LL(1) valida para expresiones.

## Que probar

- que la gramatica pueda leerse como archivo
- que sus producciones se carguen correctamente
- que no haya ambiguedades obvias en el subconjunto inicial

---

# Fase 5. Modelo interno de producciones y lector de gramatica

## Objetivo

Convertir `grammar.ll1` en estructuras de datos del compilador.

## Archivos objetivo

- `Parser/generator/production.hpp`
- `Parser/generator/grammar_reader.hpp`
- `Parser/generator/grammar_reader.cpp`

## Que hacer

1. definir una estructura `Production`
2. representar:
   - lado izquierdo
   - lado derecho
   - epsilon
3. leer el archivo `grammar.ll1`
4. cargar todas las producciones en memoria
5. identificar:
   - simbolo inicial
   - no terminales
   - terminales

## Que se aprende aqui

- como pasar de texto formal a estructuras internas
- como representar una gramatica de forma neutral

## Que probar

- leer producciones simples
- leer alternativas
- leer epsilon
- detectar errores de formato basicos

---

# Fase 6. Calculo de FIRST y FOLLOW

## Objetivo

Construir los algoritmos centrales del generador LL(1).

## Archivos objetivo

- `Parser/generator/first_follow.hpp`
- `Parser/generator/first_follow.cpp`

## Que hacer

1. calcular `FIRST(X)` para cada simbolo
2. calcular `FIRST(alpha)` para secuencias
3. calcular `FOLLOW(A)` para cada no terminal
4. manejar correctamente epsilon

## Que se aprende aqui

- la base teorica real de un parser LL(1)
- como justificar si una gramatica es apta o no

## Que probar

- FIRST de terminales
- FIRST de no terminales con epsilon
- FOLLOW del simbolo inicial
- FOLLOW en producciones encadenadas

---

# Fase 7. Construccion de la tabla LL(1)

## Objetivo

Generar la tabla predictiva a partir de la gramatica.

## Archivos objetivo

- `Parser/generator/ll1_table.hpp`
- `Parser/generator/ll1_table.cpp`

## Que hacer

1. usar producciones + FIRST + FOLLOW
2. llenar `M[A, a]`
3. detectar conflictos
4. reportar si la gramatica no es LL(1)

## Resultado esperado

Una tabla que permita parseo predictivo generico.

## Que probar

- entradas correctas para gramaticas simples
- celdas epsilon cuando corresponda
- deteccion de conflicto multiple en la misma celda

---

# Fase 8. Parser predictivo generico

## Objetivo

Crear el parser LL(1) que use la tabla en vez de funciones manuales.

## Archivos objetivo

- `Parser/syntax/ll1_parser.hpp`
- `Parser/syntax/ll1_parser.cpp`

## Que hacer

1. consumir `TokenList`
2. mapear `TokenType` a simbolos terminales de la gramatica
3. mantener una pila de parseo
4. expandir no terminales usando la tabla
5. consumir terminales
6. reportar errores sintacticos utiles

## Importante

Este parser no debe estar codificado para una sola produccion concreta.

Debe ser un parser predictivo generico controlado por:

- gramatica
- FIRST/FOLLOW
- tabla LL(1)

## Que probar

- parseo correcto de expresiones ya cubiertas por el parser recursivo
- error cuando no exista entrada en la tabla
- error cuando el terminal esperado no coincida

---

# Fase 9. Construccion de CST

## Objetivo

Construir el arbol de derivacion durante el parseo predictivo.

## Archivos objetivo

- `Parser/ast/cst_nodes.hpp`
- `Parser/ast/cst_nodes.cpp`

## Que hacer

1. modelar nodos de CST
2. distinguir:
   - nodo no terminal
   - nodo terminal
3. al expandir una produccion, crear hijos en el CST
4. al consumir un token, asociarlo al terminal correspondiente

## Por que va antes del AST

Porque en un parser generado el CST es la representacion natural de la derivacion.

## Que probar

- estructura correcta del arbol para expresiones simples
- presencia de nodos intermedios de la gramatica
- correspondencia entre derivacion y tokens consumidos

---

# Fase 10. Transformacion CST -> AST

## Objetivo

Reducir el CST a una estructura semantica compacta.

## Archivos objetivo

- `Parser/ast/expr.hpp`
- `Parser/ast/expr.cpp`
- `Parser/ast/cst_to_ast.hpp`
- `Parser/ast/cst_to_ast.cpp`

## Que hacer

1. recorrer el CST
2. eliminar nodos puramente sintacticos
3. reconstruir precedencia y estructura semantica
4. producir AST util para fases posteriores

## Importante

El AST deja de ser "algo que el parser manual construye directamente" y pasa a ser:

- una salida derivada del CST

## Que probar

- CST de `1 + 2 * 3` convertido a AST correcto
- unarios
- parentesis
- comparaciones

---

# Fase 11. Extender la gramatica del lenguaje

## Objetivo

Una vez funcionando el pipeline LL(1), crecer el lenguaje desde la gramatica, no desde codigo hardcodeado.

## Subconjuntos siguientes

1. llamadas y postfix
2. `let`
3. bloques
4. `if / elif / else`
5. `while`
6. `for`
7. programa completo y lista de sentencias
8. funciones
9. `type`, herencia, atributos, metodos, `self`, `base`, `new`

## Regla de trabajo

Cada nueva construccion debe pasar por este orden:

1. actualizar `grammar.ll1`
2. recalcular FIRST/FOLLOW
3. regenerar o reconstruir tabla LL(1)
4. verificar parseo predictivo
5. verificar CST
6. verificar conversion a AST

---

# Estructura objetivo del directorio `Parser/`

```text
Parser/
  README.md
  plan_parser.md
  explicacion.md
  core/
    token.hpp
    parse_error.hpp
    token_stream.hpp
    token_stream.cpp
    token_adapter.hpp
    token_adapter.cpp
  grammar/
    grammar.ll1
  generator/
    production.hpp
    grammar_reader.hpp
    grammar_reader.cpp
    first_follow.hpp
    first_follow.cpp
    ll1_table.hpp
    ll1_table.cpp
  syntax/
    parser.hpp
    parser.cpp
    ll1_parser.hpp
    ll1_parser.cpp
  ast/
    expr.hpp
    expr.cpp
    cst_nodes.hpp
    cst_nodes.cpp
    cst_to_ast.hpp
    cst_to_ast.cpp
  tests/
    parser_phase2_smoke.cpp
    parser_phase3_smoke.cpp
```

---

# Decision tecnica activa

Desde este punto:

- `Parser/syntax/parser.cpp` se conserva como referencia manual y banco de pruebas
- el desarrollo principal debe migrar a `grammar.ll1` + generador LL(1) + `ll1_parser`

Si hay que elegir entre:

- agregar mas reglas manuales al parser recursivo

o:

- empezar `grammar_reader`, `FIRST/FOLLOW` y tabla LL(1)

la opcion correcta para el objetivo final del proyecto es la segunda.
