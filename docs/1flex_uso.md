En Flex, una “regla” es: **patrón regex + acción C/C++**.  
Cuando el patrón hace match, ejecuta la acción (por ejemplo, `return TokenType...`).

En tu archivo [hulk_lexer.l](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Lexer/hulk_lexer.l), las reglas están entre `%% ... %%`.

## Estructura de reglas en tu lexer

1. **Macros de patrón** (alias regex)  
   Definidas arriba de `%%`:
   - `DIGIT`, `ALPHA`, `ALNUM_`, `INT`, `FLOAT`, `IDENT`  
   Sirven para reutilizar regex en reglas reales (`{FLOAT}`, `{IDENT}`, etc.).

2. **Reglas de tokens**
   Cada línea tipo:
   - `"if" { UPD(KEYWORD_IF); }`
   - `{IDENT} { ... return IDENTIFIER; }`
   - `"\n" { ++line_; column_=1; }`

3. **Regla de fallback**
   - `.` captura cualquier carácter no reconocido y devuelve `UNKNOWN`.

## Reglas definidas actualmente (por grupos)

- **Espacios y saltos**: `[ \t\r]+`, `\n`
- **Comentarios**: `"//"[^\n]*`, `"/*" ...` (escaneo manual hasta `*/`)
- **Strings**: `\" ...` con escapes (`\"`, `\\`, `\n`, `\t`, `\r`)
- **Números**: `{FLOAT}`, `{INT}`
- **Booleanos**: `"true"`, `"false"`
- **Keywords**: `if`, `elif`, `else`, `while`, `for`, `function`, `type`, `protocol`, `def`, `let`, `in`, `new`, `inherits`, `self`, `base`, `is`, `as`, `and`, `or`
- **Builtins**: `print`, `sqrt`, `sin`, `cos`, `exp`, `log`, `rand`, `PI`, `E`, `range`
- **Identificadores**: `{IDENT}`
- **Operadores/símbolos**:
  - 2 chars: `:=`, `=>`, `==`, `!=`, `<=`, `>=`
  - 1 char: `=`, `@`, `(` `)` `{` `}` `[` `]` `,` `;` `.` `:` `+` `-` `*` `/` `^` `~` `!` `<` `>`
- **EOF**: `<<EOF>>`
- **Desconocido**: `.`

## Cómo se utilizan en ejecución

`main.cpp` llama repetidamente `lexer.yylex()`; en cada llamada Flex:

1. Busca la regla que mejor matchee el texto actual.
2. Ejecuta su acción.
3. Devuelve un token (`int` de `TokenType`) o consume sin devolver (espacios/comentarios).
4. Repite hasta `EOF_TOKEN`.

Además, las acciones actualizan:
- posición (`line_`, `column_`)
- valor semántico `sem` (`str_val`, `num_val`, `bool_val`).

## Detalle importante de Flex que aplicas bien

- **Orden importa** cuando hay ambigüedad de longitud similar.
- Flex usa “**longest match**” primero.
- Por eso pones keywords y builtins antes de `{IDENT}`: así `if` no sale como identificador, sino como `KEYWORD_IF`.