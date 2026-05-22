#include <exception>
#include <iostream>
#include <string>

#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../generator/production.hpp"

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

        // Condicionales: celdas criticas de IfBody / ElifChainOpt / ElseOpt (ademas de conflicts.empty(),
        // build_ll1_table puede resolver choques epsilon vs no-epsilon sin listarlos; aqui se fija el comportamiento deseado).
        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "Program",
                "TYPE",
                "Program -> TypeDeclList StmtList EOF_TOKEN"),
            "M[Program, TYPE] starts optional class section");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "TypeParamsOpt",
                "LBRACKET",
                "TypeParamsOpt -> LBRACKET ArgIdListOpt RBRACKET"),
            "M[TypeParamsOpt, LBRACKET] uses bracketed class params");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "TypeInheritanceOpt",
                "IS",
                "TypeInheritanceOpt -> IS IDENTIFIER TypeBaseArgsOpt"),
            "M[TypeInheritanceOpt, IS] uses class inheritance alias");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "TypeAttrOrMethodTail",
                "COLON",
                "TypeAttrOrMethodTail -> TypeAnnotationOpt TypeAttributeAssign Expr SEMICOLON TypeAttrPhase"),
            "M[TypeAttrOrMethodTail, COLON] uses typed attribute production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "TypeAttrOrMethodTail",
                "LPAREN",
                "TypeAttrOrMethodTail -> LPAREN ArgIdListOpt RPAREN TypeAnnotationOpt FunctionBody TypeMethodPhase"),
            "M[TypeAttrOrMethodTail, LPAREN] starts method section");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "IfBody",
                "LBRACE",
                "IfBody -> BlockExpr"),
            "M[IfBody, LBRACE] uses block body production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "IfBody",
                "IF",
                "IfBody -> Expr"),
            "M[IfBody, IF] uses expression body production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "ElseOpt",
                "ELSE",
                "ElseOpt -> ELSE IfBody"),
            "M[ElseOpt, ELSE] uses else-branch production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "ElseOpt",
                "SEMICOLON",
                std::string("ElseOpt -> ") + parser::generator::kEpsilonSymbol),
            "M[ElseOpt, SEMICOLON] uses epsilon when else is omitted");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "ElifChainOpt",
                "ELIF",
                "ElifChainOpt -> ELIF LPAREN Expr RPAREN IfBody ElifChainOpt"),
            "M[ElifChainOpt, ELIF] continues elif chain");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "ElifChainOpt",
                "ELSE",
                std::string("ElifChainOpt -> ") + parser::generator::kEpsilonSymbol),
            "M[ElifChainOpt, ELSE] closes elif chain before optional else");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WhileBody",
                "LBRACE",
                "WhileBody -> BlockExpr"),
            "M[WhileBody, LBRACE] uses block as while body");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WhileBody",
                "NUMBER_LITERAL",
                "WhileBody -> Expr"),
            "M[WhileBody, NUMBER_LITERAL] uses expression as while body");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WhileElseOpt",
                "ELSE",
                "WhileElseOpt -> ELSE WhileBody"),
            "M[WhileElseOpt, ELSE] attaches while-else branch");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WhileElseOpt",
                "SEMICOLON",
                std::string("WhileElseOpt -> ") + parser::generator::kEpsilonSymbol),
            "M[WhileElseOpt, SEMICOLON] omits optional while-else");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "Expr",
                "WITH",
                "Expr -> WithExpr"),
            "M[Expr, WITH] dispatches to with expression");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "Expr",
                "CASE",
                "Expr -> CaseExpr"),
            "M[Expr, CASE] dispatches to case expression");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "CasePayload",
                "LBRACE",
                "CasePayload -> LBRACE CaseBranchList RBRACE"),
            "M[CasePayload, LBRACE] uses block case form");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "CasePayload",
                "IDENTIFIER",
                "CasePayload -> CaseBranch"),
            "M[CasePayload, IDENTIFIER] uses compact case form");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WithBody",
                "LBRACE",
                "WithBody -> BlockExpr"),
            "M[WithBody, LBRACE] uses block body production");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WithElseOpt",
                "ELSE",
                "WithElseOpt -> ELSE WithBody"),
            "M[WithElseOpt, ELSE] attaches with-else branch");

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "WithElseOpt",
                "SEMICOLON",
                std::string("WithElseOpt -> ") + parser::generator::kEpsilonSymbol),
            "M[WithElseOpt, SEMICOLON] omits optional with-else");

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
                "Primary",
                "NULL_LITERAL",
                "Primary -> NULL_LITERAL"),
            "M[Primary, NULL_LITERAL] uses the null literal production");

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

        ok &= expect(
            cell_has_production(
                ll1_table.table,
                "MulExprTail",
                "PERCENT",
                "MulExprTail -> PERCENT PowerExpr MulExprTail"),
            "M[MulExprTail, PERCENT] uses modulo production");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] LL(1) table smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
