#include <iostream>
#include <sstream>
#include "Lexer.hpp"

//Para correrlo
//g++ -std=c++17 -Wall -Wextra main.cpp Lexer.cpp -o lexer
//./lexer.exe(windows) o ./lexer(linux)

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::EOF_TOKEN: return "EOF";
        case TokenType::UNKNOWN: return "UNKNOWN";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::NUMBER_LITERAL: return "NUMBER";
        case TokenType::STRING_LITERAL: return "STRING";
        case TokenType::BOOLEAN_LITERAL: return "BOOLEAN";
        case TokenType::KEYWORD_IF: return "KEYWORD_IF";
        case TokenType::KEYWORD_ELIF: return "KEYWORD_ELIF";
        case TokenType::KEYWORD_ELSE: return "KEYWORD_ELSE";
        case TokenType::KEYWORD_WHILE: return "KEYWORD_WHILE";
        case TokenType::KEYWORD_FOR: return "KEYWORD_FOR";
        case TokenType::KEYWORD_FUNCTION: return "KEYWORD_FUNCTION";
        case TokenType::KEYWORD_TYPE: return "KEYWORD_TYPE";
        case TokenType::KEYWORD_PROTOCOL: return "KEYWORD_PROTOCOL";
        case TokenType::KEYWORD_DEF: return "KEYWORD_DEF";
        case TokenType::KEYWORD_LET: return "KEYWORD_LET";
        case TokenType::KEYWORD_IN: return "KEYWORD_IN";
        case TokenType::KEYWORD_NEW: return "KEYWORD_NEW";
        case TokenType::KEYWORD_INHERITS: return "KEYWORD_INHERITS";
        case TokenType::KEYWORD_SELF: return "KEYWORD_SELF";
        case TokenType::KEYWORD_BASE: return "KEYWORD_BASE";
        case TokenType::KEYWORD_IS: return "KEYWORD_IS";
        case TokenType::KEYWORD_AS: return "KEYWORD_AS";
        case TokenType::KEYWORD_AND: return "KEYWORD_AND";
        case TokenType::KEYWORD_OR: return "KEYWORD_OR";
        case TokenType::BUILTIN_PI: return "BUILTIN_PI";
        case TokenType::BUILTIN_E: return "BUILTIN_E";
        case TokenType::BUILTIN_RANGE: return "BUILTIN_RANGE";
        case TokenType::BUILTIN_PRINT: return "BUILTIN_PRINT";
        case TokenType::BUILTIN_SQRT: return "BUILTIN_SQRT";
        case TokenType::BUILTIN_SIN: return "BUILTIN_SIN";
        case TokenType::BUILTIN_COS: return "BUILTIN_COS";
        case TokenType::BUILTIN_EXP: return "BUILTIN_EXP";
        case TokenType::BUILTIN_LOG: return "BUILTIN_LOG";
        case TokenType::BUILTIN_RAND: return "BUILTIN_RAND";
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
        case TokenType::EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
    }

    return "UNKNOWN";
}

int main() {
    //aqui se prueban los casos 
   std::string code =
    "let x = new Superman() in "
    "print("
        "if (x is Bird) \"It's bird!\" "
        "elif (x is Plane) \"It's a plane!\" "
        "else \"No, it's Superman!\""
    ")";
    //arreglar los numeros de fila y columna en los tokens 
    
    std::istringstream input(code);

    /*std::string code =
        "let x := 3.14, y := PI in { "
        "print(\"hola\\n\"); "
        "if x >= y and true or false { x := x + 1; } "
        "/* comentario  "
        "function f(a: Number) => a^2; "
        "range(0,10); "
        "}";
    std::istringstream input(code);*/
   

    Lexer lexer(input);

    Token token = lexer.nextToken();

    while (token.getType() != TokenType::EOF_TOKEN) {
        std::cout << tokenTypeToString(token.getType())
                  << " -> '" << token.getValue()
                  << "' (Line: " << token.getLocation().line
                  << ", Col: " << token.getLocation().column << ")\n";

        token = lexer.nextToken();
    }

    return 0;
}
