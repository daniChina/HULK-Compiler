#include <exception>
#include <iostream>
#include <string>

#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }

    std::cout << "[OK] " << message << "\n";
    return true;
}

// Helper para validar una celda concreta M[A, a] sin repetir logica de acceso.
bool cell_has_production(
    const parser::generator::Ll1Table& table,
    const std::string& non_terminal,
    const std::string& terminal,
    const std::string& expected_production) {
    const auto row = table.find(non_terminal);
    if (row == table.end()) {
        return false;
    }

    const auto entry = row->second.find(terminal);
    if (entry == row->second.end()) {
        return false;
    }

    return parser::generator::production_to_string(entry->second) == expected_production;
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        ok &= expect(ll1_table.conflicts.empty(), "grammar builds LL(1) table without conflicts");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "Primary",
                "NUMBER_LITERAL",
                "Primary -> NUMBER_LITERAL"),
            "M[Primary, NUMBER_LITERAL] uses the number production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "Primary",
                "LPAREN",
                "Primary -> LPAREN Expr RPAREN"),
            "M[Primary, LPAREN] uses the grouped expression production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "OrExprTail",
                "OR",
                "OrExprTail -> OR AndExpr OrExprTail"),
            "M[OrExprTail, OR] uses the recursive OR production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "OrExprTail",
                "SEMICOLON",
                "OrExprTail -> ε"),
            "M[OrExprTail, SEMICOLON] uses the epsilon production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "PowerExprTail",
                "CARET",
                "PowerExprTail -> CARET PowerExpr"),
            "M[PowerExprTail, CARET] uses the power production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "PowerExprTail",
                "PLUS",
                "PowerExprTail -> ε"),
            "M[PowerExprTail, PLUS] uses epsilon after a power expression");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "PostfixTail",
                "LPAREN",
                "PostfixTail -> LPAREN ArgListOpt RPAREN PostfixTail"),
            "M[PostfixTail, LPAREN] uses call suffix production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "PostfixTail",
                "DOT",
                "PostfixTail -> DOT IDENTIFIER PostfixTail"),
            "M[PostfixTail, DOT] uses member suffix production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "PostfixTail",
                "STAR",
                "PostfixTail -> ε"),
            "M[PostfixTail, STAR] uses epsilon after completing postfix chain");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] LL(1) table smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
