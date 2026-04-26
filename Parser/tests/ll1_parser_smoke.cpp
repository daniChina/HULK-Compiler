#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

using parser::Token;
using parser::TokenList;
using parser::TokenType;

// Helper minimo para construir tokens manuales sin depender del lexer real.
Token make_token(TokenType type, std::string lexeme, int col) {
    return Token{type, std::move(lexeme), 1, col};
}

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }

    std::cout << "[OK] " << message << "\n";
    return true;
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        {
            // Caso valido: 1 + 2 * 3;
            TokenList tokens = {
                make_token(TokenType::NUMBER_LITERAL, "1", 1),
                make_token(TokenType::PLUS, "+", 3),
                make_token(TokenType::NUMBER_LITERAL, "2", 5),
                make_token(TokenType::STAR, "*", 7),
                make_token(TokenType::NUMBER_LITERAL, "3", 9),
                make_token(TokenType::SEMICOLON, ";", 10),
                make_token(TokenType::EOF_TOKEN, "", 11),
            };

            parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
            const auto result = parser.parse();
            ok &= expect(!result.derivation.empty(), "LL(1) parser accepts a valid expression statement");
        }

        {
            // Caso invalido: falta una expresion despues de '+'.
            TokenList tokens = {
                make_token(TokenType::NUMBER_LITERAL, "1", 1),
                make_token(TokenType::PLUS, "+", 3),
                make_token(TokenType::SEMICOLON, ";", 4),
                make_token(TokenType::EOF_TOKEN, "", 5),
            };

            try {
                parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
                const auto result = parser.parse();
                (void)result;
                ok &= expect(false, "LL(1) parser rejects an incomplete expression");
            } catch (const parser::ParseError&) {
                ok &= expect(true, "LL(1) parser rejects an incomplete expression");
            }
        }

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] LL(1) parser smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
