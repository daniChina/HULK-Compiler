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

        ok &= expect_program_ast(
            "while with expression body and no else",
            grammar,
            ll1_table,
            "while (false) 0;",
            "Program(\n"
            "  ExprStmt(While(Bool(false), Number(0)))\n"
            ")");

        ok &= expect_program_ast(
            "while with block body and else branch",
            grammar,
            ll1_table,
            "while (false) { 1; } else 2;",
            "Program(\n"
            "  ExprStmt(While(Bool(false), Block(Number(1)), Number(2)))\n"
            ")");

        ok &= expect_program_ast(
            "let binding with while counter pattern",
            grammar,
            ll1_table,
            "let i:Number=0 in while (i < 3) { i := i + 1; };",
            "Program(\n"
            "  ExprStmt(Let(i: Number = Number(0) in While(Binary(Identifier(i), <, Number(3)), "
            "Block(Assign(Identifier(i), :=, Binary(Identifier(i), +, Number(1)))))))\n"
            ")");

        // Estilo gcd: let con while en bloque y valor final del bloque de la funcion.
        ok &= expect_program_ast(
            "function gcd-like with let while block and trailing expression",
            grammar,
            ll1_table,
            "function gcd(a:Number, b:Number):Number {\n"
            "    let q:Number = a%b in while (q != 0) {\n"
            "        a := b;\n"
            "        b := q;\n"
            "        q := a%b;\n"
            "    };\n"
            "\n"
            "    b;\n"
            "}\n",
            "Program(\n"
            "  FunctionDecl(gcd(a: Number, b: Number): Number => Block(Let(q: Number = "
            "Binary(Identifier(a), %, Identifier(b)) in While(Binary(Identifier(q), !=, Number(0)), "
            "Block(Assign(Identifier(a), :=, Identifier(b)), Assign(Identifier(b), :=, Identifier(q)), "
            "Assign(Identifier(q), :=, Binary(Identifier(a), %, Identifier(b)))))), Identifier(b)))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] while expr pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
