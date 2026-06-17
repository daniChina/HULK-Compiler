#include <exception>
#include <iostream>
#include <set>
#include <string>

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

bool contains_production(
    const parser::generator::Grammar& grammar,
    const std::string& expected) {
    for (const auto& production : grammar.productions) {
        if (parser::generator::production_to_string(production) == expected) {
            return true;
        }
    }

    return false;
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");

        bool ok = true;
        ok &= expect(grammar.start_symbol == "Program", "start symbol is Program");
        ok &= expect(!grammar.productions.empty(), "grammar has productions");
        ok &= expect(grammar.non_terminals.count("Expr") == 1, "Expr recognized as non-terminal");
        ok &= expect(grammar.non_terminals.count("Primary") == 1, "Primary recognized as non-terminal");
        ok &= expect(grammar.terminals.count("NUMBER_LITERAL") == 1, "NUMBER_LITERAL recognized as terminal");
        ok &= expect(grammar.terminals.count("CARET") == 1, "CARET recognized as terminal");
        ok &= expect(contains_production(grammar, "Program -> StmtList EOF_TOKEN"),
                     "Program production loaded with unified statement list");
        ok &= expect(contains_production(grammar, "Stmt -> BlockStmt"),
                     "top-level block statement production loaded");
        ok &= expect(contains_production(grammar, "PowerExprTail -> CARET PowerExpr"),
                     "right-associative power production loaded");
        ok &= expect(contains_production(grammar, "WithExpr -> WITH LPAREN WithSourceExpr AS IDENTIFIER RPAREN WithBody WithElseOpt"),
                     "with null-safety production loaded");
        ok &= expect(contains_production(grammar, "CaseExpr -> CASE Expr OF CasePayload"),
                     "case expression production loaded");
        ok &= expect(contains_production(grammar, "ClassInheritanceOpt -> INHERITS IDENTIFIER ClassBaseArgsOpt"),
                     "type inheritance with inherits (matcom)");
        ok &= expect(contains_production(grammar, "ClassBaseArgsOpt -> LPAREN ArgListOpt RPAREN"),
                     "parent constructor args use parentheses only (B8)");
        ok &= expect(contains_production(grammar, "ClassParamsOpt -> LPAREN ArgIdListOpt RPAREN"),
                     "type parameter parenthesized form loaded");
        ok &= expect(contains_production(grammar, "ClassDecl -> TYPE IDENTIFIER ClassParamsOpt ClassInheritanceOpt LBRACE ClassBody RBRACE"),
                     "type declaration without semicolon after RBRACE (B6)");
        ok &= expect(contains_production(grammar, "ClassBody -> ClassAttrList ClassMethodList"),
                     "type body splits attributes and methods (B3)");
        ok &= expect(contains_production(grammar, "ClassAttrListHead -> TypeAnnotation EQUAL Expr SEMICOLON"),
                     "typed class attribute with = only (B4)");
        ok &= expect(contains_production(grammar, "ClassMethod -> IDENTIFIER LPAREN ArgIdListOpt RPAREN MethodReturnTypeOpt MethodBody"),
                     "class method uses matcom return type and body forms");
        ok &= expect(contains_production(grammar, "MethodBody -> BlockExpr"),
                     "method block body production loaded");
        ok &= expect(contains_production(grammar, "OrExprTail -> ε"),
                     "epsilon alternative loaded");
        ok &= expect(contains_production(grammar, "Primary -> LPAREN Expr RPAREN"),
                     "grouped primary production loaded");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] grammar reader threw exception: " << error.what() << "\n";
        return 1;
    }
}
