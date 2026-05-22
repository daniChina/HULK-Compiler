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
        ok &= expect(contains_production(grammar, "Program -> TypeDeclList StmtList EOF_TOKEN"),
                     "Program production loaded with type section before statement list");
        ok &= expect(contains_production(grammar, "PowerExprTail -> CARET PowerExpr"),
                     "right-associative power production loaded");
        ok &= expect(contains_production(grammar, "WithExpr -> WITH LPAREN WithSourceExpr AS IDENTIFIER RPAREN WithBody WithElseOpt"),
                     "with null-safety production loaded");
        ok &= expect(contains_production(grammar, "CaseExpr -> CASE Expr OF CasePayload"),
                     "case expression production loaded");
        ok &= expect(contains_production(grammar, "TypeInheritanceOpt -> IS IDENTIFIER TypeBaseArgsOpt"),
                     "class inheritance alias with is loaded");
        ok &= expect(contains_production(grammar, "TypeParamsOpt -> LBRACKET ArgIdListOpt RBRACKET"),
                     "class parameter bracket form loaded");
        ok &= expect(contains_production(grammar, "TypeAttrOrMethodTail -> TypeAnnotationOpt TypeAttributeAssign Expr SEMICOLON TypeAttrPhase"),
                     "typed attribute production loaded");
        ok &= expect(contains_production(grammar, "TypeMethodPhase -> IDENTIFIER LPAREN ArgIdListOpt RPAREN TypeAnnotationOpt FunctionBody TypeMethodPhase"),
                     "method-only phase after first method loaded");
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
