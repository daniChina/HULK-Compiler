#include <exception>
#include <iostream>
#include <string>

#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }

    std::cout << "[OK] " << message << "\n";
    return true;
}

bool contains_all(
    const parser::generator::SymbolSet& symbols,
    std::initializer_list<std::string> expected) {
    for (const auto& item : expected) {
        if (symbols.count(item) == 0) {
            return false;
        }
    }

    return true;
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);

        bool ok = true;

        ok &= expect(
            contains_all(
                first_follow.first_sets.at("Primary"),
                {"NUMBER_LITERAL", "STRING_LITERAL", "TRUE", "FALSE", "IDENTIFIER", "LPAREN"}),
            "FIRST(Primary) contains all primary starters");

        ok &= expect(
            contains_all(
                first_follow.first_sets.at("UnaryExpr"),
                {"MINUS", "BANG", "TILDE", "NUMBER_LITERAL", "STRING_LITERAL", "TRUE", "FALSE", "IDENTIFIER", "LPAREN"}),
            "FIRST(UnaryExpr) contains unary operators and primary starters");

        ok &= expect(
            contains_all(
                first_follow.first_sets.at("PostfixTail"),
                {"LPAREN", "DOT", parser::generator::kEpsilonSymbol}),
            "FIRST(PostfixTail) contains call, member access and epsilon");

        ok &= expect(
            first_follow.first_sets.at("OrExprTail").count(parser::generator::kEpsilonSymbol) == 1,
            "FIRST(OrExprTail) contains epsilon");

        ok &= expect(
            contains_all(
                first_follow.follow_sets.at("Expr"),
                {"SEMICOLON", "RPAREN"}),
            "FOLLOW(Expr) contains semicolon and right paren");

        ok &= expect(
            contains_all(
                first_follow.follow_sets.at("ExprStmt"),
                {"EOF_TOKEN"}),
            "FOLLOW(ExprStmt) contains EOF");

        ok &= expect(
            contains_all(
                first_follow.follow_sets.at("Primary"),
                {"LPAREN", "DOT", "CARET", "STAR", "SLASH", "PLUS", "MINUS", "CONCAT", "CONCAT_WS", "LESS", "LESS_EQUAL",
                 "GREATER", "GREATER_EQUAL", "EQUAL_EQUAL", "BANG_EQUAL", "AND", "OR", "SEMICOLON", "RPAREN"}),
            "FOLLOW(Primary) contains the expected operator and closing symbols");

        ok &= expect(
            contains_all(
                first_follow.follow_sets.at("Expr"),
                {"COMMA", "RPAREN", "SEMICOLON"}),
            "FOLLOW(Expr) contains separators used in calls and statements");

        {
            const auto first_sequence = parser::generator::compute_first_of_sequence(
                {"UnaryExpr", "PowerExprTail"},
                first_follow.first_sets,
                grammar);

            ok &= expect(
                contains_all(
                    first_sequence,
                    {"MINUS", "BANG", "TILDE", "NUMBER_LITERAL", "STRING_LITERAL", "TRUE", "FALSE", "IDENTIFIER", "LPAREN"}),
                "FIRST(UnaryExpr PowerExprTail) matches expression starters");
        }

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] first/follow smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
