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

bool expect_error(const std::string& name, TokenList tokens) {
    try {
        parser::Parser parser(std::move(tokens));
        auto expr = parser.parse_expression_statement();
        std::cerr << "[FAIL] " << name << "\n"
                  << "  se esperaba error y se obtuvo: " << parser::expr_to_string(*expr) << "\n";
        return false;
    } catch (const parser::ParseError& error) {
        std::cout << "[OK] " << name << " -> " << error.what() << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  excepcion inesperada: " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_expression(
        "number literal",
        {
            make_token(TokenType::NUMBER_LITERAL, "42", 1),
            make_token(TokenType::SEMICOLON, ";", 3),
            make_token(TokenType::EOF_TOKEN, "", 4),
        },
        "Number(42)");

    ok &= expect_expression(
        "string literal",
        {
            make_token(TokenType::STRING_LITERAL, "hola", 1),
            make_token(TokenType::SEMICOLON, ";", 7),
            make_token(TokenType::EOF_TOKEN, "", 8),
        },
        "String(\"hola\")");

    ok &= expect_expression(
        "true literal",
        {
            make_token(TokenType::TRUE, "true", 1),
            make_token(TokenType::SEMICOLON, ";", 5),
            make_token(TokenType::EOF_TOKEN, "", 6),
        },
        "Bool(true)");

    ok &= expect_expression(
        "identifier",
        {
            make_token(TokenType::IDENTIFIER, "x", 1),
            make_token(TokenType::SEMICOLON, ";", 2),
            make_token(TokenType::EOF_TOKEN, "", 3),
        },
        "Identifier(x)");

    ok &= expect_expression(
        "grouped number",
        {
            make_token(TokenType::LPAREN, "(", 1),
            make_token(TokenType::NUMBER_LITERAL, "42", 2),
            make_token(TokenType::RPAREN, ")", 4),
            make_token(TokenType::SEMICOLON, ";", 5),
            make_token(TokenType::EOF_TOKEN, "", 6),
        },
        "Grouped(Number(42))");

    ok &= expect_error(
        "missing closing paren",
        {
            make_token(TokenType::LPAREN, "(", 1),
            make_token(TokenType::NUMBER_LITERAL, "42", 2),
            make_token(TokenType::SEMICOLON, ";", 4),
            make_token(TokenType::EOF_TOKEN, "", 5),
        });

    return ok ? 0 : 1;
}
