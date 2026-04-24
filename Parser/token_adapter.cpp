#include "token_adapter.hpp"

#include <fstream>
#include <stdexcept>

#include "../Lexer/hulk_lexer.h"

namespace parser {

TokenType from_lexer_token(::TokenType lexer_type) {
    switch (lexer_type) {
        case ::TokenType::EOF_TOKEN:      return TokenType::EOF_TOKEN;
        case ::TokenType::UNKNOWN:        return TokenType::UNKNOWN;
        case ::TokenType::IDENTIFIER:     return TokenType::IDENTIFIER;
        case ::TokenType::NUMBER_LITERAL: return TokenType::NUMBER_LITERAL;
        case ::TokenType::STRING_LITERAL: return TokenType::STRING_LITERAL;
        case ::TokenType::TRUE:           return TokenType::TRUE;
        case ::TokenType::FALSE:          return TokenType::FALSE;
        case ::TokenType::IF:             return TokenType::IF;
        case ::TokenType::ELIF:           return TokenType::ELIF;
        case ::TokenType::ELSE:           return TokenType::ELSE;
        case ::TokenType::WHILE:          return TokenType::WHILE;
        case ::TokenType::FOR:            return TokenType::FOR;
        case ::TokenType::FUNCTION:       return TokenType::FUNCTION;
        case ::TokenType::TYPE:           return TokenType::TYPE;
        case ::TokenType::PROTOCOL:       return TokenType::PROTOCOL;
        case ::TokenType::DEF:            return TokenType::DEF;
        case ::TokenType::LET:            return TokenType::LET;
        case ::TokenType::IN:             return TokenType::IN;
        case ::TokenType::NEW:            return TokenType::NEW;
        case ::TokenType::INHERITS:       return TokenType::INHERITS;
        case ::TokenType::SELF:           return TokenType::SELF;
        case ::TokenType::BASE:           return TokenType::BASE;
        case ::TokenType::IS:             return TokenType::IS;
        case ::TokenType::AS:             return TokenType::AS;
        case ::TokenType::AND:            return TokenType::AND;
        case ::TokenType::OR:             return TokenType::OR;
        case ::TokenType::LPAREN:         return TokenType::LPAREN;
        case ::TokenType::RPAREN:         return TokenType::RPAREN;
        case ::TokenType::LBRACE:         return TokenType::LBRACE;
        case ::TokenType::RBRACE:         return TokenType::RBRACE;
        case ::TokenType::LBRACKET:       return TokenType::LBRACKET;
        case ::TokenType::RBRACKET:       return TokenType::RBRACKET;
        case ::TokenType::COMMA:          return TokenType::COMMA;
        case ::TokenType::SEMICOLON:      return TokenType::SEMICOLON;
        case ::TokenType::DOT:            return TokenType::DOT;
        case ::TokenType::COLON:          return TokenType::COLON;
        case ::TokenType::PLUS:           return TokenType::PLUS;
        case ::TokenType::MINUS:          return TokenType::MINUS;
        case ::TokenType::STAR:           return TokenType::STAR;
        case ::TokenType::SLASH:          return TokenType::SLASH;
        case ::TokenType::CARET:          return TokenType::CARET;
        case ::TokenType::TILDE:          return TokenType::TILDE;
        case ::TokenType::BANG:           return TokenType::BANG;
        case ::TokenType::ASSIGN:         return TokenType::ASSIGN;
        case ::TokenType::ARROW:          return TokenType::ARROW;
        case ::TokenType::CONCAT:         return TokenType::CONCAT;
        case ::TokenType::CONCAT_WS:      return TokenType::CONCAT_WS;
        case ::TokenType::EQUAL:          return TokenType::EQUAL;
        case ::TokenType::EQUAL_EQUAL:    return TokenType::EQUAL_EQUAL;
        case ::TokenType::BANG_EQUAL:     return TokenType::BANG_EQUAL;
        case ::TokenType::LESS:           return TokenType::LESS;
        case ::TokenType::LESS_EQUAL:     return TokenType::LESS_EQUAL;
        case ::TokenType::GREATER:        return TokenType::GREATER;
        case ::TokenType::GREATER_EQUAL:  return TokenType::GREATER_EQUAL;
        default:
            throw std::runtime_error("Token del lexer no mapeado al contrato del parser");
    }
}

Token make_token(const HulkLexer& lexer, ::TokenType lexer_type) {
    return Token{
        from_lexer_token(lexer_type),
        lexer.lexeme(),
        lexer.token_line(),
        lexer.token_col()
    };
}

TokenList tokenize_stream(std::istream& input) {
    HulkLexer lexer(input);
    TokenList tokens;

    while (true) {
        const auto raw_type = static_cast<::TokenType>(lexer.yylex());
        tokens.push_back(make_token(lexer, raw_type));
        if (raw_type == ::TokenType::EOF_TOKEN) {
            break;
        }
    }

    return tokens;
}

TokenList tokenize_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir archivo para tokenizar: " + path);
    }

    return tokenize_stream(file);
}

}  // namespace parser
