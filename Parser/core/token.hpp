#pragma once

#include <string>
#include <vector>

namespace parser {

enum class TokenType {
    EOF_TOKEN,
    UNKNOWN,

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

    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    COMMA,
    SEMICOLON,
    DOT,
    COLON,

    PLUS,
    MINUS,
    STAR,
    SLASH,
    CARET,
    TILDE,
    BANG,

    ASSIGN,
    ARROW,
    CONCAT,
    CONCAT_WS,

    EQUAL,
    EQUAL_EQUAL,
    BANG_EQUAL,
    LESS,
    LESS_EQUAL,
    GREATER,
    GREATER_EQUAL
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int col;
};

using TokenList = std::vector<Token>;

inline const char* token_name(TokenType type) {
    switch (type) {
        case TokenType::EOF_TOKEN: return "EOF_TOKEN";
        case TokenType::UNKNOWN: return "UNKNOWN";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER_LITERAL: return "NUMBER_LITERAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::IF: return "IF";
        case TokenType::ELIF: return "ELIF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::FOR: return "FOR";
        case TokenType::FUNCTION: return "FUNCTION";
        case TokenType::TYPE: return "TYPE";
        case TokenType::PROTOCOL: return "PROTOCOL";
        case TokenType::DEF: return "DEF";
        case TokenType::LET: return "LET";
        case TokenType::IN: return "IN";
        case TokenType::NEW: return "NEW";
        case TokenType::INHERITS: return "INHERITS";
        case TokenType::SELF: return "SELF";
        case TokenType::BASE: return "BASE";
        case TokenType::IS: return "IS";
        case TokenType::AS: return "AS";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";
        case TokenType::COLON: return "COLON";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::CARET: return "CARET";
        case TokenType::TILDE: return "TILDE";
        case TokenType::BANG: return "BANG";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::ARROW: return "ARROW";
        case TokenType::CONCAT: return "CONCAT";
        case TokenType::CONCAT_WS: return "CONCAT_WS";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        default: return "?";
    }
}

}  // namespace parser
