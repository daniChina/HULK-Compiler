#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "../ast/cst_to_ast.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

using parser::Token;
using parser::TokenList;
using parser::TokenType;

// Helper minimo para construir tokens de prueba.
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
            // Caso valido: desde el CST de 1 + 2 * 3; debe salir el AST con precedencia correcta.
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
            const auto parse_result = parser.parse();
            auto ast = parser::cst_to_ast(*parse_result.cst_root);

            ok &= expect(
                parser::expr_to_string(*ast) ==
                    "Binary(Number(1), +, Binary(Number(2), *, Number(3)))",
                "CST converts to AST with correct multiplication precedence");
        }

        {
            // Caso valido: potencia asociativa a la derecha y agrupacion.
            TokenList tokens = {
                make_token(TokenType::LPAREN, "(", 1),
                make_token(TokenType::NUMBER_LITERAL, "2", 2),
                make_token(TokenType::PLUS, "+", 4),
                make_token(TokenType::NUMBER_LITERAL, "3", 6),
                make_token(TokenType::RPAREN, ")", 7),
                make_token(TokenType::CARET, "^", 9),
                make_token(TokenType::NUMBER_LITERAL, "4", 11),
                make_token(TokenType::SEMICOLON, ";", 12),
                make_token(TokenType::EOF_TOKEN, "", 13),
            };

            parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
            const auto parse_result = parser.parse();
            auto ast = parser::cst_to_ast(*parse_result.cst_root);

            ok &= expect(
                parser::expr_to_string(*ast) ==
                    "Binary(Grouped(Binary(Number(2), +, Number(3))), ^, Number(4))",
                "CST converts grouped expression and power to the expected AST");
        }

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] CST to AST smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
