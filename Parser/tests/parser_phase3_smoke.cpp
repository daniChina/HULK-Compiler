#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "../ast/expr.hpp"
#include "../syntax/parser.hpp"

namespace {

using parser::Token;
using parser::TokenList;
using parser::TokenType;

Token make_token(TokenType type, std::string lexeme, int col) {
    return Token{type, std::move(lexeme), 1, col};
}

bool expect_expression(const std::string& name, TokenList tokens, const std::string& expected) {
    try {
        parser::Parser parser(std::move(tokens));
        auto expr = parser.parse_expression_statement();
        const auto actual = parser::expr_to_string(*expr);
        if (actual != expected) {
            std::cerr << "[FAIL] " << name << "\n"
                      << "  esperado: " << expected << "\n"
                      << "  obtenido: " << actual << "\n";
            return false;
        }
        std::cout << "[OK] " << name << " -> " << actual << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  excepcion: " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_expression(
        "add and multiply precedence",
        {
            make_token(TokenType::NUMBER_LITERAL, "1", 1),
            make_token(TokenType::PLUS, "+", 3),
            make_token(TokenType::NUMBER_LITERAL, "2", 5),
            make_token(TokenType::STAR, "*", 7),
            make_token(TokenType::NUMBER_LITERAL, "3", 9),
            make_token(TokenType::SEMICOLON, ";", 10),
            make_token(TokenType::EOF_TOKEN, "", 11),
        },
        "Binary(Number(1), +, Binary(Number(2), *, Number(3)))");

    ok &= expect_expression(
        "and before or",
        {
            make_token(TokenType::IDENTIFIER, "a", 1),
            make_token(TokenType::AND, "and", 3),
            make_token(TokenType::IDENTIFIER, "b", 7),
            make_token(TokenType::OR, "or", 9),
            make_token(TokenType::IDENTIFIER, "c", 12),
            make_token(TokenType::SEMICOLON, ";", 13),
            make_token(TokenType::EOF_TOKEN, "", 14),
        },
        "Binary(Binary(Identifier(a), and, Identifier(b)), or, Identifier(c))");

    ok &= expect_expression(
        "concat precedence",
        {
            make_token(TokenType::STRING_LITERAL, "a", 1),
            make_token(TokenType::CONCAT, "@", 5),
            make_token(TokenType::STRING_LITERAL, "b", 7),
            make_token(TokenType::SEMICOLON, ";", 10),
            make_token(TokenType::EOF_TOKEN, "", 11),
        },
        "Binary(String(\"a\"), @, String(\"b\"))");

    ok &= expect_expression(
        "comparison chain",
        {
            make_token(TokenType::IDENTIFIER, "x", 1),
            make_token(TokenType::LESS, "<", 3),
            make_token(TokenType::IDENTIFIER, "y", 5),
            make_token(TokenType::EQUAL_EQUAL, "==", 7),
            make_token(TokenType::IDENTIFIER, "z", 10),
            make_token(TokenType::SEMICOLON, ";", 11),
            make_token(TokenType::EOF_TOKEN, "", 12),
        },
        "Binary(Binary(Identifier(x), <, Identifier(y)), ==, Identifier(z))");

    ok &= expect_expression(
        "unary minus",
        {
            make_token(TokenType::MINUS, "-", 1),
            make_token(TokenType::IDENTIFIER, "x", 2),
            make_token(TokenType::SEMICOLON, ";", 3),
            make_token(TokenType::EOF_TOKEN, "", 4),
        },
        "Unary(-, Identifier(x))");

    ok &= expect_expression(
        "grouped addition before multiply",
        {
            make_token(TokenType::LPAREN, "(", 1),
            make_token(TokenType::NUMBER_LITERAL, "1", 2),
            make_token(TokenType::PLUS, "+", 4),
            make_token(TokenType::NUMBER_LITERAL, "2", 6),
            make_token(TokenType::RPAREN, ")", 7),
            make_token(TokenType::STAR, "*", 9),
            make_token(TokenType::NUMBER_LITERAL, "3", 11),
            make_token(TokenType::SEMICOLON, ";", 12),
            make_token(TokenType::EOF_TOKEN, "", 13),
        },
        "Binary(Grouped(Binary(Number(1), +, Number(2))), *, Number(3))");

    ok &= expect_expression(
        "modulo shares multiplicative precedence",
        {
            make_token(TokenType::NUMBER_LITERAL, "10", 1),
            make_token(TokenType::PERCENT, "%", 4),
            make_token(TokenType::NUMBER_LITERAL, "3", 6),
            make_token(TokenType::STAR, "*", 8),
            make_token(TokenType::NUMBER_LITERAL, "2", 10),
            make_token(TokenType::SEMICOLON, ";", 11),
            make_token(TokenType::EOF_TOKEN, "", 12),
        },
        "Binary(Binary(Number(10), %, Number(3)), *, Number(2))");

    ok &= expect_expression(
        "power is right associative",
        {
            make_token(TokenType::NUMBER_LITERAL, "2", 1),
            make_token(TokenType::CARET, "^", 3),
            make_token(TokenType::NUMBER_LITERAL, "3", 5),
            make_token(TokenType::CARET, "^", 7),
            make_token(TokenType::NUMBER_LITERAL, "4", 9),
            make_token(TokenType::SEMICOLON, ";", 10),
            make_token(TokenType::EOF_TOKEN, "", 11),
        },
        "Binary(Number(2), ^, Binary(Number(3), ^, Number(4)))");

    ok &= expect_expression(
        "assignment is right associative and weaker than addition",
        {
            make_token(TokenType::IDENTIFIER, "y", 1),
            make_token(TokenType::ASSIGN, ":=", 3),
            make_token(TokenType::IDENTIFIER, "x", 6),
            make_token(TokenType::ASSIGN, ":=", 8),
            make_token(TokenType::NUMBER_LITERAL, "5", 11),
            make_token(TokenType::PLUS, "+", 13),
            make_token(TokenType::NUMBER_LITERAL, "5", 15),
            make_token(TokenType::SEMICOLON, ";", 16),
            make_token(TokenType::EOF_TOKEN, "", 17),
        },
        "Binary(Identifier(y), :=, Binary(Identifier(x), :=, Binary(Number(5), +, Number(5))))");

    ok &= expect_expression(
        "parenthesized assignment before addition",
        {
            make_token(TokenType::IDENTIFIER, "y", 1),
            make_token(TokenType::ASSIGN, ":=", 3),
            make_token(TokenType::LPAREN, "(", 5),
            make_token(TokenType::IDENTIFIER, "x", 6),
            make_token(TokenType::ASSIGN, ":=", 8),
            make_token(TokenType::NUMBER_LITERAL, "5", 11),
            make_token(TokenType::RPAREN, ")", 12),
            make_token(TokenType::PLUS, "+", 14),
            make_token(TokenType::NUMBER_LITERAL, "1", 16),
            make_token(TokenType::SEMICOLON, ";", 17),
            make_token(TokenType::EOF_TOKEN, "", 18),
        },
        "Binary(Identifier(y), :=, Binary(Grouped(Binary(Identifier(x), :=, Number(5))), +, Number(1)))");

    return ok ? 0 : 1;
}
