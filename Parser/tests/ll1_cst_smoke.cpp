#include <exception>
#include <iostream>
#include <string>
#include <utility>

#include "../ast/cst_nodes.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

using parser::CstNode;
using parser::Token;
using parser::TokenList;
using parser::TokenType;

// Helper minimo para construir tokens manuales en la prueba.
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

// Busca recursivamente el primer nodo con cierto simbolo.
const CstNode* find_symbol(const CstNode& node, const std::string& expected_symbol) {
    if (node.symbol == expected_symbol) {
        return &node;
    }

    for (const auto& child : node.children) {
        if (const CstNode* found = find_symbol(*child, expected_symbol)) {
            return found;
        }
    }

    return nullptr;
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        {
            // Caso valido: el parser debe construir un CST real para 1 + 2 * 3;
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

            ok &= expect(result.cst_root != nullptr, "LL(1) parser builds a CST root");
            ok &= expect(result.cst_root->symbol == "Program", "CST root symbol is Program");
            ok &= expect(find_symbol(*result.cst_root, "ExprStmt") != nullptr, "CST contains ExprStmt");

            const CstNode* number_node = find_symbol(*result.cst_root, "NUMBER_LITERAL");
            ok &= expect(number_node != nullptr, "CST contains NUMBER_LITERAL");
            ok &= expect(number_node != nullptr && number_node->has_token, "NUMBER_LITERAL stores the matched token");
            ok &= expect(number_node != nullptr && number_node->token.lexeme == "1",
                         "NUMBER_LITERAL token keeps the original lexeme");
        }

        {
            // Caso invalido: falta una expresion despues de '+' y por tanto no debe construir parseo valido.
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
                ok &= expect(false, "LL(1) parser rejects CST construction for an incomplete expression");
            } catch (const parser::ParseError&) {
                ok &= expect(true, "LL(1) parser rejects CST construction for an incomplete expression");
            }
        }

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] LL(1) CST smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
