# Phase 00: Lexer Errors

Esta fase contiene casos disenados para validar exclusivamente el comportamiento del lexer.

## Que debe probar esta fase

- caracteres desconocidos
- cadenas sin cerrar
- comentarios de bloque sin cerrar
- escapes validos e invalidos
- operadores compuestos del lenguaje
- separacion correcta entre tokens parecidos
- contrato lexico actual: builtins como `IDENTIFIER`, booleanos como `TRUE` y `FALSE`

## Que no debe probar esta fase

No mezclar aqui errores sintacticos, por ejemplo:

- `1 + ;`
- `print((1 + 2);`
- `if (true) 1;`

Esos casos pertenecen al parser, no al lexer.

## Convencion

Cada archivo incluye comentarios HULK (`// ...`) explicando que deberia pasar cuando el lexer procese ese input.
