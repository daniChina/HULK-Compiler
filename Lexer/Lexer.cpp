#include "Lexer.hpp"
#include <cctype>

Lexer::Lexer(std::istream& in)
    : input(in), line(1), column(0), currentChar('\0')
{
    keywords = {
        {"if", TokenType::KEYWORD_IF},
        {"elif", TokenType::KEYWORD_ELIF},
        {"else", TokenType::KEYWORD_ELSE},
        {"while", TokenType::KEYWORD_WHILE},
        {"for", TokenType::KEYWORD_FOR},
        {"function", TokenType::KEYWORD_FUNCTION},
        {"type", TokenType::KEYWORD_TYPE},
        {"protocol", TokenType::KEYWORD_PROTOCOL},
        {"def", TokenType::KEYWORD_DEF},
        {"let", TokenType::KEYWORD_LET},
        {"in", TokenType::KEYWORD_IN},
        {"new", TokenType::KEYWORD_NEW},
        {"inherits", TokenType::KEYWORD_INHERITS},
        {"self", TokenType::KEYWORD_SELF},
        {"base", TokenType::KEYWORD_BASE},
        {"is", TokenType::KEYWORD_IS},
        {"as", TokenType::KEYWORD_AS},
        {"and", TokenType::KEYWORD_AND},
        {"or", TokenType::KEYWORD_OR},
        {"PI", TokenType::BUILTIN_PI},
        {"E", TokenType::BUILTIN_E},
        {"range", TokenType::BUILTIN_RANGE},
        {"print", TokenType::BUILTIN_PRINT},
        {"sqrt", TokenType::BUILTIN_SQRT},
        {"sin", TokenType::BUILTIN_SIN},
        {"cos", TokenType::BUILTIN_COS},
        {"exp", TokenType::BUILTIN_EXP},
        {"log", TokenType::BUILTIN_LOG},
        {"rand", TokenType::BUILTIN_RAND}
    };

    advance();
}

void Lexer::advance() {
    const int next = input.get();
    if (next == EOF) {
        currentChar = '\0';
        return;
    }

    currentChar = static_cast<char>(next);
    if (currentChar == '\n') {
        line++;
        column = 0;
    } else {
        column++;
    }
}

char Lexer::peek() {
    const int next = input.peek();
    if (next == EOF) {
        return '\0';
    }
    return static_cast<char>(next);
}

void Lexer::skipWhitespace() {
    while (currentChar != '\0' && std::isspace(static_cast<unsigned char>(currentChar))) {
        advance();
    }
}

void Lexer::skipIgnored() {
    while (true) {
        skipWhitespace();

        if (currentChar == '/' && peek() == '/') {
            while (currentChar != '\0' && currentChar != '\n') {
                advance();
            }
            continue;
        }

        if (currentChar == '/' && peek() == '*') {
            advance(); // /
            advance(); // *
            while (currentChar != '\0') {
                if (currentChar == '*' && peek() == '/') {
                    advance(); // *
                    advance(); // /
                    break;
                }
                advance();
            }
            continue;
        }

        break;
    }
}

void Lexer::applyVirtualNewline(TokenType type) {
    const bool isDelimiter =
        type == TokenType::SEMICOLON ||
        type == TokenType::LBRACE ||
        type == TokenType::RBRACE;

    if (!isDelimiter) {
        return;
    }

    // If the source has no physical newline after a statement/block delimiter,
    // split logical lines so diagnostics are not concentrated on line 1.
    if (currentChar != '\n' && currentChar != '\0') {
        line++;
        column = 0;
    }
}

Token Lexer::simpleToken(TokenType type, const std::string& value) {
    SourceLocation loc{line, column};
    advance();
    applyVirtualNewline(type);
    return Token(type, value, loc);
}

Token Lexer::identifierOrKeyword() {
    std::string result;
    SourceLocation loc{line, column};

    while (std::isalnum(currentChar) || currentChar == '_') {
        result += currentChar;
        advance();
    }

    // Boolean literals
    if (result == "true" || result == "false") {
        return Token(TokenType::BOOLEAN_LITERAL, result, loc);
    }

    // Keywords
    if (keywords.count(result)) {
        return Token(keywords[result], result, loc);
    }

    return Token(TokenType::IDENTIFIER, result, loc);
}

Token Lexer::number() {
    std::string result;
    SourceLocation loc{line, column};

    while (std::isdigit(static_cast<unsigned char>(currentChar))) {
        result += currentChar;
        advance();
    }

    if (currentChar == '.' && std::isdigit(static_cast<unsigned char>(peek()))) {
        result += currentChar;
        advance();

        while (std::isdigit(static_cast<unsigned char>(currentChar))) {
            result += currentChar;
            advance();
        }
    }

    return Token(TokenType::NUMBER_LITERAL, result, loc);
}

Token Lexer::stringLiteral() {
    std::string result;
    SourceLocation loc{line, column};

    advance(); // opening quote

    while (currentChar != '"' && currentChar != '\0') {
        if (currentChar == '\\') {
            advance();
            switch (currentChar) {
                case 'n':
                    result += '\n';
                    break;
                case 't':
                    result += '\t';
                    break;
                case '"':
                    result += '"';
                    break;
                case '\\':
                    result += '\\';
                    break;
                default:
                    result += currentChar;
                    break;
            }
            advance();
            continue;
        }

        result += currentChar;
        advance();
    }

    if (currentChar == '"') {
        advance(); // closing quote
    }

    return Token(TokenType::STRING_LITERAL, result, loc);
}

Token Lexer::nextToken() {
    skipIgnored();

    if (currentChar == '\0') {
        return Token(TokenType::EOF_TOKEN, "", {line, column});
    }

    if (std::isalpha(static_cast<unsigned char>(currentChar)) || currentChar == '_') {
        return identifierOrKeyword();
    }

    if (std::isdigit(static_cast<unsigned char>(currentChar))) {
        return number();
    }

    if (currentChar == '"') {
        return stringLiteral();
    }

    switch (currentChar) {
        case '(':
            return simpleToken(TokenType::LPAREN, "(");
        case ')':
            return simpleToken(TokenType::RPAREN, ")");
        case '{':
            return simpleToken(TokenType::LBRACE, "{");
        case '}':
            return simpleToken(TokenType::RBRACE, "}");
        case '[':
            return simpleToken(TokenType::LBRACKET, "[");
        case ']':
            return simpleToken(TokenType::RBRACKET, "]");
        case ',':
            return simpleToken(TokenType::COMMA, ",");
        case ';':
            return simpleToken(TokenType::SEMICOLON, ";");
        case '.':
            return simpleToken(TokenType::DOT, ".");
        case ':':
            if (peek() == '=') {
                SourceLocation loc{line, column};
                advance();
                advance();
                return Token(TokenType::ASSIGN, ":=", loc);
            }
            return simpleToken(TokenType::COLON, ":");
        case '+':
            return simpleToken(TokenType::PLUS, "+");
        case '-':
            return simpleToken(TokenType::MINUS, "-");
        case '*':
            return simpleToken(TokenType::STAR, "*");
        case '/':
            return simpleToken(TokenType::SLASH, "/");
        case '^':
            return simpleToken(TokenType::CARET, "^");
        case '~':
            return simpleToken(TokenType::TILDE, "~");
        case '=':
            if (peek() == '=') {
                SourceLocation loc{line, column};
                advance();
                advance();
                return Token(TokenType::EQUAL_EQUAL, "==", loc);
            }
            if (peek() == '>') {
                SourceLocation loc{line, column};
                advance();
                advance();
                return Token(TokenType::ARROW, "=>", loc);
            }
            break;
        case '!':
            if (peek() == '=') {
                SourceLocation loc{line, column};
                advance();
                advance();
                return Token(TokenType::BANG_EQUAL, "!=", loc);
            }
            return simpleToken(TokenType::BANG, "!");
        case '<':
            if (peek() == '=') {
                SourceLocation loc{line, column};
                advance();
                advance();
                return Token(TokenType::LESS_EQUAL, "<=", loc);
            }
            return simpleToken(TokenType::LESS, "<");
        case '>':
            if (peek() == '=') {
                SourceLocation loc{line, column};
                advance();
                advance();
                return Token(TokenType::GREATER_EQUAL, ">=", loc);
            }
            return simpleToken(TokenType::GREATER, ">");
        default:
            break;
    }

    SourceLocation loc{line, column};
    char unknown = currentChar;
    advance();
    return Token(TokenType::UNKNOWN, std::string(1, unknown), loc);
}
