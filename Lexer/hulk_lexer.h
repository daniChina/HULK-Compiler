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
    TRUE,
    FALSE,
    IF,
    ELIF,
    ELSE,
    WHILE,
    FOR,
    FUNCTION,
    TYPE,
    PROTOCOL,
    DEF,
    LET,
    IN,
    NEW,
    INHERITS,
    SELF,
    BASE,
    IS,
    AS,
    AND,
    OR,
    LPAREN, RPAREN,
    LBRACE, RBRACE,
    LBRACKET, RBRACKET,
    COMMA, SEMICOLON, DOT, COLON,
    PLUS, MINUS, STAR, SLASH, CARET, TILDE, BANG,
    ASSIGN,
    ARROW,
    CONCAT,
    CONCAT_WS,
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
    std::string lexeme_;    /* lexema crudo del token actual      */
    int line_   = 1;        /* línea (1-based)                    */
    int column_ = 1;        /* columna (1-based)                  */
    int token_line_ = 1;    /* línea inicial del token actual     */
    int token_col_  = 1;    /* columna inicial del token actual   */

    explicit HulkLexer(std::istream& in  = std::cin,
                       std::ostream& out = std::cout)
        : yyFlexLexer(&in, &out) {}

    /* Declaración; la definición la genera flex++ */
    virtual int yylex() override;

    int  line() const { return line_;   }
    int  col()  const { return column_; }
    const std::string& lexeme() const { return lexeme_; }
    int  token_line() const { return token_line_; }
    int  token_col()  const { return token_col_;  }

    static const char* token_name(int t) {
        switch (static_cast<TokenType>(t)) {
            case TokenType::EOF_TOKEN:        return "EOF_TOKEN";
            case TokenType::UNKNOWN:          return "UNKNOWN";
            case TokenType::IDENTIFIER:       return "IDENTIFIER";
            case TokenType::NUMBER_LITERAL:   return "NUMBER_LITERAL";
            case TokenType::STRING_LITERAL:   return "STRING_LITERAL";
            case TokenType::TRUE:             return "TRUE";
            case TokenType::FALSE:            return "FALSE";
            case TokenType::IF:               return "IF";
            case TokenType::ELIF:             return "ELIF";
            case TokenType::ELSE:             return "ELSE";
            case TokenType::WHILE:            return "WHILE";
            case TokenType::FOR:              return "FOR";
            case TokenType::FUNCTION:         return "FUNCTION";
            case TokenType::TYPE:             return "TYPE";
            case TokenType::PROTOCOL:         return "PROTOCOL";
            case TokenType::DEF:              return "DEF";
            case TokenType::LET:              return "LET";
            case TokenType::IN:               return "IN";
            case TokenType::NEW:              return "NEW";
            case TokenType::INHERITS:         return "INHERITS";
            case TokenType::SELF:             return "SELF";
            case TokenType::BASE:             return "BASE";
            case TokenType::IS:               return "IS";
            case TokenType::AS:               return "AS";
            case TokenType::AND:              return "AND";
            case TokenType::OR:               return "OR";
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
            case TokenType::CONCAT_WS:        return "CONCAT_WS";
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
