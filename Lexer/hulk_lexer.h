#pragma once
/* ================================================================
 * hulk_lexer.h  —  Declaración de HulkLexer para el lexer HULK
 *
 * IMPORTANTE: Este header debe incluirse en el prólogo (%{...%})
 * del archivo .l ANTES del primer %%, para que la clase esté
 * declarada cuando flex++ genere el cuerpo de yylex().
 *
 * El guard __FLEX_LEXER_H evita doble-inclusión de FlexLexer.h:
 * flex++ ya lo incluye internamente; sin el guard se obtiene
 * "redefinition of class yyFlexLexer".
 * ================================================================ */

#ifndef __FLEX_LEXER_H
#  include <FlexLexer.h>
#endif

#include <iostream>
#include <string>
#include <cstdlib>
#include <cstring>

/* ── Tipos de token ──────────────────────────────────────── */
enum class TokenType : int {
    EOF_TOKEN        = 0,
    UNKNOWN          = 256,
    IDENTIFIER,
    NUMBER_LITERAL,
    STRING_LITERAL,
    BOOLEAN_LITERAL,
    KEYWORD_IF,
    KEYWORD_ELIF,
    KEYWORD_ELSE,
    KEYWORD_WHILE,
    KEYWORD_FOR,
    KEYWORD_FUNCTION,
    KEYWORD_TYPE,
    KEYWORD_PROTOCOL,
    KEYWORD_DEF,
    KEYWORD_LET,
    KEYWORD_IN,
    KEYWORD_NEW,
    KEYWORD_INHERITS,
    KEYWORD_SELF,
    KEYWORD_BASE,
    KEYWORD_IS,
    KEYWORD_AS,
    KEYWORD_AND,
    KEYWORD_OR,
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
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    COMMA, SEMICOLON, DOT, COLON,
    PLUS, MINUS, STAR, SLASH, CARET, TILDE, BANG,
    ASSIGN,
    ARROW,
    CONCAT,
    EQUAL_EQUAL, BANG_EQUAL,EQUAL,
    LESS, LESS_EQUAL,
    GREATER, GREATER_EQUAL
};

/* ── Valor semántico ─────────────────────────────────────── */
struct SemanticValue {
    std::string str_val;
    float       num_val  = 0.0f;
    bool        bool_val = false;
};

/* ================================================================
 * HulkLexer — subclase de yyFlexLexer
 *
 * Con %option yyclass="HulkLexer" flex++ emite:
 *   #define YY_DECL int HulkLexer::yylex()
 * y por tanto las acciones de las reglas se compilan como parte
 * de HulkLexer::yylex(). Así `this` es HulkLexer* y los miembros
 * line_, column_ y sem son accesibles directamente.
 * ================================================================ */
class HulkLexer : public yyFlexLexer {
public:
    SemanticValue sem;      /* valor semántico del token actual   */
    int line_   = 1;        /* línea (1-based)                    */
    int column_ = 1;        /* columna (1-based)                  */

    explicit HulkLexer(std::istream& in  = std::cin,
                       std::ostream& out = std::cout)
        : yyFlexLexer(&in, &out) {}

    /* Declaración; la definición la genera flex++ */
    virtual int yylex() override;

    int  line() const { return line_;   }
    int  col()  const { return column_; }

    static const char* token_name(int t) {
        switch (static_cast<TokenType>(t)) {
            case TokenType::EOF_TOKEN:        return "EOF";
            case TokenType::UNKNOWN:          return "UNKNOWN";
            case TokenType::IDENTIFIER:       return "IDENTIFIER";
            case TokenType::NUMBER_LITERAL:   return "NUMBER_LITERAL";
            case TokenType::STRING_LITERAL:   return "STRING_LITERAL";
            case TokenType::BOOLEAN_LITERAL:  return "BOOLEAN_LITERAL";
            case TokenType::KEYWORD_IF:       return "KEYWORD_IF";
            case TokenType::KEYWORD_ELIF:     return "KEYWORD_ELIF";
            case TokenType::KEYWORD_ELSE:     return "KEYWORD_ELSE";
            case TokenType::KEYWORD_WHILE:    return "KEYWORD_WHILE";
            case TokenType::KEYWORD_FOR:      return "KEYWORD_FOR";
            case TokenType::KEYWORD_FUNCTION: return "KEYWORD_FUNCTION";
            case TokenType::KEYWORD_TYPE:     return "KEYWORD_TYPE";
            case TokenType::KEYWORD_PROTOCOL: return "KEYWORD_PROTOCOL";
            case TokenType::KEYWORD_DEF:      return "KEYWORD_DEF";
            case TokenType::KEYWORD_LET:      return "KEYWORD_LET";
            case TokenType::KEYWORD_IN:       return "KEYWORD_IN";
            case TokenType::KEYWORD_NEW:      return "KEYWORD_NEW";
            case TokenType::KEYWORD_INHERITS: return "KEYWORD_INHERITS";
            case TokenType::KEYWORD_SELF:     return "KEYWORD_SELF";
            case TokenType::KEYWORD_BASE:     return "KEYWORD_BASE";
            case TokenType::KEYWORD_IS:       return "KEYWORD_IS";
            case TokenType::KEYWORD_AS:       return "KEYWORD_AS";
            case TokenType::KEYWORD_AND:      return "KEYWORD_AND";
            case TokenType::KEYWORD_OR:       return "KEYWORD_OR";
            case TokenType::BUILTIN_PI:       return "BUILTIN_PI";
            case TokenType::BUILTIN_E:        return "BUILTIN_E";
            case TokenType::BUILTIN_RANGE:    return "BUILTIN_RANGE";
            case TokenType::BUILTIN_PRINT:    return "BUILTIN_PRINT";
            case TokenType::BUILTIN_SQRT:     return "BUILTIN_SQRT";
            case TokenType::BUILTIN_SIN:      return "BUILTIN_SIN";
            case TokenType::BUILTIN_COS:      return "BUILTIN_COS";
            case TokenType::BUILTIN_EXP:      return "BUILTIN_EXP";
            case TokenType::BUILTIN_LOG:      return "BUILTIN_LOG";
            case TokenType::BUILTIN_RAND:     return "BUILTIN_RAND";
            case TokenType::LPAREN:           return "LPAREN";
            case TokenType::RPAREN:           return "RPAREN";
            case TokenType::LBRACE:           return "LBRACE";
            case TokenType::RBRACE:           return "RBRACE";
            case TokenType::LBRACKET:         return "LBRACKET";
            case TokenType::RBRACKET:         return "RBRACKET";
            case TokenType::COMMA:            return "COMMA";
            case TokenType::SEMICOLON:        return "SEMICOLON";
            case TokenType::DOT:              return "DOT";
            case TokenType::COLON:            return "COLON";
            case TokenType::PLUS:             return "PLUS";
            case TokenType::MINUS:            return "MINUS";
            case TokenType::STAR:             return "STAR";
            case TokenType::SLASH:            return "SLASH";
            case TokenType::CARET:            return "CARET";
            case TokenType::TILDE:            return "TILDE";
            case TokenType::BANG:             return "BANG";
            case TokenType::ASSIGN:           return "ASSIGN";
            case TokenType::CONCAT:           return "CONCAT";
            case TokenType::ARROW:            return "ARROW";
            case TokenType::EQUAL:            return "EQUAL";
            case TokenType::EQUAL_EQUAL:      return "EQUAL_EQUAL";
            case TokenType::BANG_EQUAL:       return "BANG_EQUAL";
            case TokenType::LESS:             return "LESS";
            case TokenType::LESS_EQUAL:       return "LESS_EQUAL";
            case TokenType::GREATER:          return "GREATER";
            case TokenType::GREATER_EQUAL:    return "GREATER_EQUAL";
            default:                          return "???";
        }
    }
};
