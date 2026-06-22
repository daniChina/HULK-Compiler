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

Token make_token(TokenType type, std::string lexeme, int line, int col) {
    return Token{type, std::move(lexeme), line, col};
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
        const auto& stmt_first = first_follow.first_sets.at("Stmt");

        bool ok = true;

        {
            TokenList tokens = {
                make_token(TokenType::NUMBER_LITERAL, "1", 1, 1),
                make_token(TokenType::SEMICOLON, ";", 1, 2),
                make_token(TokenType::NUMBER_LITERAL, "2", 2, 1),
                make_token(TokenType::EOF_TOKEN, "", 2, 2),
            };

            parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table, &stmt_first);
            const auto result = parser.parse_with_recovery();
            ok &= expect(result.recovered, "recovery reports error on missing semicolon");
            ok &= expect(result.errors.size() == 1, "recovery collects one syntax error");
            ok &= expect(result.cst_root != nullptr, "recovery keeps partial CST root");
        }

        {
            TokenList tokens = {
                make_token(TokenType::NUMBER_LITERAL, "1", 1, 1),
                make_token(TokenType::SEMICOLON, ";", 1, 2),
                make_token(TokenType::PLUS, "+", 2, 1),
                make_token(TokenType::SEMICOLON, ";", 2, 2),
                make_token(TokenType::NUMBER_LITERAL, "3", 3, 1),
                make_token(TokenType::SEMICOLON, ";", 3, 2),
                make_token(TokenType::EOF_TOKEN, "", 3, 3),
            };

            parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table, &stmt_first);
            const auto result = parser.parse_with_recovery();
            ok &= expect(result.recovered, "recovery continues after broken stmt");
            ok &= expect(result.errors.size() == 1, "recovery reports one broken stmt");
            ok &= expect(!result.derivation.empty(), "recovery still derives valid stmts");
        }

        {
            TokenList tokens = {
                make_token(TokenType::NUMBER_LITERAL, "7", 1, 1),
                make_token(TokenType::SEMICOLON, ";", 1, 2),
                make_token(TokenType::EOF_TOKEN, "", 1, 3),
            };

            parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table, &stmt_first);
            const auto stmt = parser.parse_stmt();
            ok &= expect(stmt != nullptr && stmt->symbol == "Stmt", "parse_stmt builds one Stmt node");
        }

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] LL(1) recovery smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
