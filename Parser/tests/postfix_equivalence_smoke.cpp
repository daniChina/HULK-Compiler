
#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "../ast/cst_to_ast.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

bool expect_program_ast(
    const std::string& name,
    const parser::generator::Grammar& grammar,
    const parser::generator::Ll1TableResult& ll1_table,
    const std::string& source,
    const std::string& expected_program) {
    try {
        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        auto program = parser::cst_to_ast(*parse_result.cst_root);
        const auto actual_program = parser::program_to_string(*program);

        if (actual_program != expected_program) {
            std::cerr << "[FAIL] " << name << "\n"
                      << "  esperado: " << expected_program << "\n"
                      << "  obtenido: " << actual_program << "\n";
            return false;
        }

        std::cout << "[OK] " << name << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  excepcion: " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        // §10.2 #1–2: llamada global
        ok &= expect_program_ast(
            "global call f(a,b) -> CallExpr",
            grammar,
            ll1_table,
            "f(a, b);",
            "Program(\n"
            "  ExprStmt(Call(Identifier(f), [Identifier(a), Identifier(b)]))\n"
            ")");

        ok &= expect_program_ast(
            "global call no args",
            grammar,
            ll1_table,
            "print();",
            "Program(\n"
            "  ExprStmt(Call(Identifier(print), []))\n"
            ")");

        // §10.2 #3: lectura de miembro
        ok &= expect_program_ast(
            "member read obj.attr -> GetAttrExpr",
            grammar,
            ll1_table,
            "obj.value;",
            "Program(\n"
            "  ExprStmt(GetAttr(Identifier(obj), value))\n"
            ")");

        ok &= expect_program_ast(
            "method call obj.m(a) -> Call(GetAttr(...))",
            grammar,
            ll1_table,
            "obj.m(a);",
            "Program(\n"
            "  ExprStmt(Call(GetAttr(Identifier(obj), m), [Identifier(a)]))\n"
            ")");

        ok &= expect_program_ast(
            "method call no args",
            grammar,
            ll1_table,
            "p.norm();",
            "Program(\n"
            "  ExprStmt(Call(GetAttr(Identifier(p), norm), []))\n"
            ")");

        ok &= expect_program_ast(
            "member assign obj.x := v -> Assign(GetAttr)",
            grammar,
            ll1_table,
            "box.x := 1;",
            "Program(\n"
            "  ExprStmt(Assign(GetAttr(Identifier(box), x), :=, Number(1)))\n"
            ")");

        // §10.2 #7: cadena a.b(c).d
        ok &= expect_program_ast(
            "chained postfix a.b(c).d",
            grammar,
            ll1_table,
            "a.b(c).d;",
            "Program(\n"
            "  ExprStmt(GetAttr(Call(GetAttr(Identifier(a), b), [Identifier(c)]), d))\n"
            ")");

        // §10.2 #8: new T().m()
        ok &= expect_program_ast(
            "new then method postfix",
            grammar,
            ll1_table,
            "new Point(1, 2).length();",
            "Program(\n"
            "  ExprStmt(Call(GetAttr(New(Point(Number(1), Number(2))), length), []))\n"
            ")");

        // §10.2 #9: self.x
        ok &= expect_program_ast(
            "self member access",
            grammar,
            ll1_table,
            "self.x;",
            "Program(\n"
            "  ExprStmt(GetAttr(Self, x))\n"
            ")");

        // §10.2 #10: builtin parse(42)
        ok &= expect_program_ast(
            "builtin as identifier call",
            grammar,
            ll1_table,
            "parse(42);",
            "Program(\n"
            "  ExprStmt(Call(Identifier(parse), [Number(42)]))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] postfix equivalence smoke threw exception: "
                  << error.what() << "\n";
        return 1;
    }
}
