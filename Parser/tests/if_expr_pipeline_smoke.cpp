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

        // Sin else: el AST deja else_branch ausente (valor Null solo a nivel semantico).
        ok &= expect_program_ast(
            "if without else",
            grammar,
            ll1_table,
            "if (true) 1;",
            "Program(\n"
            "  ExprStmt(If(Bool(true), Number(1)))\n"
            ")");

        ok &= expect_program_ast(
            "if with else simple expressions",
            grammar,
            ll1_table,
            "if (true) 1 else 0;",
            "Program(\n"
            "  ExprStmt(If(Bool(true), Number(1), Number(0)))\n"
            ")");

        ok &= expect_program_ast(
            "if elif else desugars to nested If in AST",
            grammar,
            ll1_table,
            "if (x < 0) -1 elif (x == 0) 0 else 1;",
            "Program(\n"
            "  ExprStmt(If(Binary(Identifier(x), <, Number(0)), Unary(-, Number(1)), "
            "If(Binary(Identifier(x), ==, Number(0)), Number(0), Number(1))))\n"
            ")");

        ok &= expect_program_ast(
            "multiple elif without final else",
            grammar,
            ll1_table,
            "if (false) 0 elif (false) 0 elif (true) 3;",
            "Program(\n"
            "  ExprStmt(If(Bool(false), Number(0), If(Bool(false), Number(0), If(Bool(true), Number(3)))))\n"
            ")");

        ok &= expect_program_ast(
            "if and else branches can be blocks",
            grammar,
            ll1_table,
            "if (true) { print(1); } else { print(2); };",
            "Program(\n"
            "  ExprStmt(If(Bool(true), Block(Call(Identifier(print), [Number(1)])), "
            "Block(Call(Identifier(print), [Number(2)]))))\n"
            ")");

        ok &= expect_program_ast(
            "mixed block then-branch and expression else-branch",
            grammar,
            ll1_table,
            "if (true) { 1; } else 2;",
            "Program(\n"
            "  ExprStmt(If(Bool(true), Block(Number(1)), Number(2)))\n"
            ")");

        ok &= expect_program_ast(
            "fib-like function body uses if expression",
            grammar,
            ll1_table,
            "function fib(n:Number):Number -> if (n <= 1) 1 else fib(n-1) + fib(n-2);\n"
            "print(fib(5));",
            "Program(\n"
            "  FunctionDecl(fib(n: Number): Number => If(Binary(Identifier(n), <=, Number(1)), Number(1), "
            "Binary(Call(Identifier(fib), [Binary(Identifier(n), -, Number(1))]), +, "
            "Call(Identifier(fib), [Binary(Identifier(n), -, Number(2))]))))\n"
            "  ExprStmt(Call(Identifier(print), [Call(Identifier(fib), [Number(5)])]))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] if expr pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
