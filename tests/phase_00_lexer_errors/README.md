# Phase 00: Lexer Errors

Esta fase contiene casos diseñados para validar exclusivamente el comportamiento del lexer.

## Qué debe probar esta fase

- caracteres desconocidos
- cadenas sin cerrar
- comentarios de bloque sin cerrar
- escapes válidos e inválidos
- operadores compuestos del lenguaje
- separación correcta entre tokens parecidos

## Qué no debe probar esta fase

No mezclar aquí errores sintácticos, por ejemplo:

- `1 + ;`
- `print((1 + 2);`
- `if (true) 1;`

Esos casos pertenecen al parser, no al lexer.

## Convención

Cada archivo incluye comentarios HULK (`// ...`) explicando qué debería pasar cuando el lexer procese ese input.
