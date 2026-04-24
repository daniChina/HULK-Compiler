Sí: en este caso, el `.hpp` declara la clase y el `.cpp` implementa su comportamiento.

No es una “interfaz” en el sentido estricto de Java o C#; en C++ es más correcto decir:

- el `.hpp` contiene la **declaración pública** de `TokenStream`
- el `.cpp` contiene la **definición/implementación** de sus métodos

Voy por partes.

---

# 1. Diferencia entre `.hpp` y `.cpp`

## `.hpp`
Normalmente contiene:
- declaraciones de clases
- firmas de métodos
- tipos
- includes necesarios para usar esa API

En tu caso:
- [token_stream.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/token_stream.hpp)

le dice al resto del proyecto:

- existe una clase `TokenStream`
- tiene estos métodos
- tiene estos atributos privados

Pero ahí no está la lógica detallada de casi ningún método.

---

## `.cpp`
Normalmente contiene:
- la implementación real de los métodos declarados en el `.hpp`

En tu caso:
- [token_stream.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/token_stream.cpp)

ahí está el código que dice exactamente:
- cómo se mueve `index_`
- qué devuelve `current()`
- qué hace `advance()`
- cuándo `consume()` lanza `ParseError`

---

## Entonces, ¿`TokenStream` en el `.hpp` es una interfaz?
No exactamente.

En C++ una interfaz pura suele ser algo como:

```cpp
class IParser {
public:
    virtual void parse() = 0;
    virtual ~IParser() = default;
};
```

Eso sería una interfaz abstracta.

Tu `TokenStream` no es eso.
Tu `TokenStream` es una **clase concreta**:
- tiene estado (`tokens_`, `index_`)
- tiene comportamiento implementado en el `.cpp`
- se puede instanciar directamente

O sea:
- no es una interfaz pura
- sí es la **API pública** de la clase

---

# 2. Explicación del `.hpp`

Archivo: [token_stream.hpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/token_stream.hpp)

```cpp
#pragma once
```

Evita que el archivo se incluya múltiples veces en la misma compilación.

---

```cpp
#include <cstddef>
#include <string>
```

Trae definiciones necesarias:
- `std::size_t`
- `std::string`

---

```cpp
#include "parse_error.hpp"
#include "token.hpp"
```

Necesita:
- `ParseError`
- `Token`
- `TokenType`
- `TokenList`

---

```cpp
namespace parser {
```

Todo lo que sigue vive dentro del namespace `parser`.

Sirve para evitar conflictos con otros nombres globales.

---

```cpp
class TokenStream {
```

Declara la clase.

Esta clase representa una secuencia navegable de tokens.

---

```cpp
public:
```

Empieza la parte pública: lo que otras partes del código pueden usar.

---

```cpp
explicit TokenStream(TokenList tokens);
```

Constructor.

Recibe una lista de tokens y construye el flujo.

`explicit` evita conversiones implícitas raras.

Ejemplo:
```cpp
TokenStream stream(tokens);
```

---

```cpp
const Token& current() const;
```

Devuelve una referencia al token actual.

- `const Token&`: no copia el token
- `const` al final: no modifica el estado del objeto

---

```cpp
const Token& peek(std::size_t offset = 1) const;
```

Mira un token hacia adelante sin consumirlo.

Por defecto `offset = 1`, o sea, mira el siguiente.

---

```cpp
const Token& previous() const;
```

Devuelve el token anterior, es decir, el último consumido.

---

```cpp
const Token& advance();
```

Avanza la posición actual y devuelve el token que acaba de consumir.

---

```cpp
bool is(TokenType type) const;
```

Pregunta si el token actual tiene ese tipo.

---

```cpp
bool match(TokenType type);
```

Si el token actual coincide:
- consume
- devuelve `true`

Si no coincide:
- no consume
- devuelve `false`

---

```cpp
const Token& consume(TokenType type, const std::string& message);
```

Si el token actual es del tipo esperado:
- lo consume
- lo devuelve

Si no:
- lanza error con el mensaje dado

---

```cpp
bool at_end() const;
```

Indica si ya estás en `EOF_TOKEN`.

---

```cpp
std::size_t position() const { return index_; }
```

Devuelve el índice interno actual.

Este sí está implementado en el `.hpp` directamente porque es trivial.

---

```cpp
private:
```

Empieza la parte privada: estado interno que el usuario de la clase no debe tocar directamente.

---

```cpp
TokenList tokens_;
```

Aquí se guarda la secuencia completa de tokens.

---

```cpp
std::size_t index_ = 0;
```

Índice del token actual.

Al inicio apunta al primer token.

---

```cpp
};
```

Fin de la clase.

---

```cpp
}  // namespace parser
```

Fin del namespace.

---

# 3. Explicación del `.cpp`

Archivo: [token_stream.cpp](/home/nebur02/Documents/3er Ano/2do SEMESTRE/COMPILACION/proyecto/HULK-Compiler/Parser/token_stream.cpp)

---

```cpp
#include "token_stream.hpp"
```

Incluye la declaración de la clase para implementar sus métodos.

---

```cpp
#include <algorithm>
#include <utility>
```

Se usan para:
- `std::min`
- `std::move`

---

```cpp
namespace parser {
```

Las implementaciones pertenecen al mismo namespace.

---

## Constructor

```cpp
TokenStream::TokenStream(TokenList tokens)
    : tokens_(std::move(tokens)) {
```

Recibe la lista por valor y luego la mueve a `tokens_`.

`std::move` evita copias innecesarias.

---

```cpp
    if (tokens_.empty()) {
        tokens_.push_back(Token{TokenType::EOF_TOKEN, "", 1, 1});
    }
}
```

Si la lista llega vacía, mete un `EOF_TOKEN`.

### Por qué
Porque el flujo siempre debe tener al menos un token válido.
Si no, `current()` podría romperse al acceder a un vector vacío.

---

## `current()`

```cpp
const Token& TokenStream::current() const {
    return tokens_[std::min(index_, tokens_.size() - 1)];
}
```

Devuelve el token actual.

### Detalle importante
Usa:

```cpp
std::min(index_, tokens_.size() - 1)
```

para no salirte del vector.

Si por alguna razón `index_` ya pasó del final, devuelve el último token disponible, que idealmente es `EOF_TOKEN`.

---

## `peek()`

```cpp
const Token& TokenStream::peek(std::size_t offset) const {
    return tokens_[std::min(index_ + offset, tokens_.size() - 1)];
}
```

Mira adelante sin consumir.

Ejemplo:
- `peek(1)` mira el siguiente
- `peek(2)` mira dos posiciones adelante

También usa `std::min` para no salirse.

---

## `previous()`

```cpp
const Token& TokenStream::previous() const {
    if (index_ == 0) {
        return tokens_.front();
    }
    return tokens_[index_ - 1];
}
```

Si todavía estás al inicio (`index_ == 0`), devuelve el primero.

Si no, devuelve el token anterior al actual.

### Por qué hace falta
Después de `advance()`, normalmente quieres recuperar el token consumido.

---

## `advance()`

```cpp
const Token& TokenStream::advance() {
    if (!at_end()) {
        ++index_;
    }
    return previous();
}
```

Si no estás en EOF:
- incrementa `index_`

Luego devuelve el token anterior, que es justamente el que acabas de consumir.

### Ejemplo
Si estabas en:
- `IDENTIFIER`

y llamas `advance()`:
- `index_` pasa al siguiente
- devuelve el `IDENTIFIER` consumido

---

## `is()`

```cpp
bool TokenStream::is(TokenType type) const {
    return current().type == type;
}
```

Comparación simple del tipo actual con el esperado.

---

## `match()`

```cpp
bool TokenStream::match(TokenType type) {
    if (!is(type)) {
        return false;
    }
    advance();
    return true;
}
```

Si el token actual no coincide:
- devuelve `false`

Si sí coincide:
- consume con `advance()`
- devuelve `true`

### Para qué sirve
Para elementos opcionales.

Ejemplo futuro:
```cpp
if (stream.match(TokenType::ELSE)) { ... }
```

---

## `consume()`

```cpp
const Token& TokenStream::consume(TokenType type, const std::string& message) {
    if (!is(type)) {
        throw ParseError(current(), message);
    }
    return advance();
}
```

Si no coincide el token esperado:
- lanza `ParseError`

Si sí coincide:
- consume y devuelve el token

### Para qué sirve
Para elementos obligatorios.

Ejemplo futuro:
```cpp
stream.consume(TokenType::RPAREN, "se esperaba ')'");
```

---

## `at_end()`

```cpp
bool TokenStream::at_end() const {
    return current().type == TokenType::EOF_TOKEN;
}
```

Devuelve verdadero si el token actual es EOF.

---

```cpp
}  // namespace parser
```

Fin del namespace.

---

# 4. Qué es lo que “se implementa” en el `.cpp`

En el `.hpp` declaraste:
- qué métodos existen
- qué reciben
- qué devuelven

En el `.cpp` implementaste:
- cómo acceden a `tokens_`
- cómo usan `index_`
- cuándo avanzan
- cómo hacen lookahead
- cuándo lanzan errores

O sea:

## `.hpp`
Define la forma pública de la clase.

## `.cpp`
Define el comportamiento real de la clase.

---

# 5. Modelo mental de `TokenStream`

Piensa que tienes una lista así:

```text
0: IDENTIFIER("print")
1: LPAREN("(")
2: NUMBER_LITERAL("42")
3: RPAREN(")")
4: SEMICOLON(";")
5: EOF_TOKEN("")
```

e `index_ = 0`.

## `current()`
devuelve:
- token 0

## `peek()`
devuelve:
- token 1

## `advance()`
mueve `index_` a 1 y devuelve token 0

## `current()` ahora
devuelve token 1

## `consume(LPAREN, ...)`
verifica que token 1 sea `LPAREN`, lo consume y deja el índice en 2

Ese es exactamente el mecanismo base de un parser recursivo.

---

# 6. Por qué esta clase es tan importante
Porque casi todo el parser va a escribirse en términos de esta API.

Más adelante vas a tener funciones como:

```cpp
parse_primary()
parse_expression()
parse_if_expr()
parse_let_expr()
```

Y dentro harán cosas como:

```cpp
if (stream.match(TokenType::TRUE)) ...
if (stream.match(TokenType::FALSE)) ...
if (stream.match(TokenType::IDENTIFIER)) ...
stream.consume(TokenType::RPAREN, "se esperaba ')'");
```

Si `TokenStream` está bien diseñado, el resto del parser se vuelve mucho más claro.

---

# 7. Observación importante sobre tu código actual
Tu `TokenStream` ya está bien para fase 1.

Lo único a tener en mente es que:
- `current()` y `peek()` usan `std::min(...)`
- eso está bien para protegerte de accesos fuera de rango
- pero presupone que el último token de la lista es `EOF_TOKEN`

Y en tu adaptador eso ya se cumple.

---

Si quieres, en el siguiente paso te explico igual de detallado `ParseError` y luego pasamos a `parse_primary()` en la fase 2.