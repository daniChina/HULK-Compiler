#pragma once
#include <istream>
#include <unordered_map>
#include "Token.hpp"

class Lexer {
private:
    std::istream& input;
    int line;
    int column;
    char currentChar;

    enum class State {
        START,
        ID_OR_KEYWORD,
        NUMBER,
        STRING,
        OPERATOR
    };

    std::unordered_map<std::string, TokenType> keywords;

    void advance();
    void skipWhitespace();
    void skipIgnored();
    char peek();
    void applyVirtualNewline(TokenType type);

    Token identifierOrKeyword();
    Token number();
    Token stringLiteral();
    Token simpleToken(TokenType type, const std::string& value);

public:
    Lexer(std::istream& in);

    Token nextToken();
};
