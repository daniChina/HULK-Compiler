#pragma once
#include <string>
#include <memory>

enum class TokenType {
    // Special
    EOF_TOKEN,
    UNKNOWN,

    // Identifiers & literals
    IDENTIFIER,
    NUMBER_LITERAL,
    STRING_LITERAL,
    BOOLEAN_LITERAL,

    // Keywords (control flow)
    KEYWORD_IF,
    KEYWORD_ELIF,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_FOR,

    // Keywords (declarations)
    KEYWORD_FUNCTION,
    KEYWORD_TYPE,
    KEYWORD_PROTOCOL,
    KEYWORD_DEF,

    // Keywords (scope)
    KEYWORD_LET,
    KEYWORD_IN,

    // Keywords (OO / typing)
    KEYWORD_NEW,
    KEYWORD_INHERITS,
    KEYWORD_SELF,
    KEYWORD_BASE,
    KEYWORD_IS,
    KEYWORD_AS,
    KEYWORD_AND,
    KEYWORD_OR,

    // Builtins & special names
    BUILTIN_PI,
    BUILTIN_E,
    BUILTIN_RANGE,
    BUILTIN_PRINT,
    BUILTIN_SQRT,
    BUILTIN_SIN,
    BUILTIN_COS,
    BUILTIN_EXP,
    BUILTIN_LOG,
    BUILTIN_RAND,

    // Symbols
    LPAREN,     // (
    RPAREN,     // )
    LBRACE,     // {
    RBRACE,     // }
    LBRACKET,   // [
    RBRACKET,   // ]
    COMMA,      // ,
    SEMICOLON,  // ;
    DOT,        // .
    COLON,      // :

    // Operators
    PLUS,           // +
    MINUS,          // -
    STAR,           // *
    SLASH,          // /
    CARET,          // ^
    TILDE,          // ~
    BANG,           // !
    ASSIGN,         // :=
    ARROW,          // =>
    EQUAL_EQUAL,    // ==
    BANG_EQUAL,     // !=
    LESS,           // <
    LESS_EQUAL,     // <=
    GREATER,        // >
    GREATER_EQUAL   // >=
};

struct SourceLocation {
    int line;
    int column;
};

class Token {
protected:
    TokenType type;
    std::string value;
    SourceLocation location;

public:
    Token(TokenType type, const std::string& value, SourceLocation loc)
        : type(type), value(value), location(loc) {}

    virtual ~Token() = default;

    TokenType getType() const { return type; }
    const std::string& getValue() const { return value; }
    SourceLocation getLocation() const { return location; }
};
