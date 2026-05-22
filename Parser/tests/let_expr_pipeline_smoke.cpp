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

// Helper comun para comparar el AST final producido por todo el pipeline.
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

        // Caso 1: forma minima de let con una sola variable y cuerpo expresion.
        ok &= expect_program_ast(
            "single let binding with expression body",
            grammar,
            ll1_table,
            "let x = 1 in x + 1;",
            "Program(\n"
            "  ExprStmt(Let(x = Number(1) in Binary(Identifier(x), +, Number(1))))\n"
            ")");

        // Caso 2: let con anotacion de tipo opcional en el binding.
        ok &= expect_program_ast(
            "typed let binding",
            grammar,
            ll1_table,
            "let msg: String = \"Hola Mundo\" in print(msg);",
            "Program(\n"
            "  ExprStmt(Let(msg: String = String(\"\"Hola Mundo\"\") in Call(Identifier(print), [Identifier(msg)])))\n"
            ")");

        // Caso 3: multiples bindings deben desazucararse como lets anidados.
        ok &= expect_program_ast(
            "multiple let bindings become nested lets",
            grammar,
            ll1_table,
            "let x = 1, y = 2, z = 3 in x + (y * z);",
            "Program(\n"
            "  ExprStmt(Let(x = Number(1) in Let(y = Number(2) in Let(z = Number(3) in Binary(Identifier(x), +, Grouped(Binary(Identifier(y), *, Identifier(z))))))))\n"
            ")");

        // Caso 4: cuerpo en bloque dentro del let para validar la variante
        // donde el body es una lista de expresiones entre llaves.
        ok &= expect_program_ast(
            "let body can be a block expression list",
            grammar,
            ll1_table,
            "let x = 0 in { print(x == 0); print(x == 1); };",
            "Program(\n"
            "  ExprStmt(Let(x = Number(0) in Block(Call(Identifier(print), [Binary(Identifier(x), ==, Number(0))]), Call(Identifier(print), [Binary(Identifier(x), ==, Number(1))]))))\n"
            ")");

        // Caso 5: tres lets escritos ya anidados explicitamente.
        ok &= expect_program_ast(
            "three explicit nested lets",
            grammar,
            ll1_table,
            "let a = 1 in let b = 2 in let c = 3 in a + b + c;",
            "Program(\n"
            "  ExprStmt(Let(a = Number(1) in Let(b = Number(2) in Let(c = Number(3) in Binary(Binary(Identifier(a), +, Identifier(b)), +, Identifier(c))))))\n"
            ")");

        // Caso 6: mezcla de lets anidados con tipos y bloque final.
        ok &= expect_program_ast(
            "three nested lets with types and block body",
            grammar,
            ll1_table,
            "let x: Number = 1 in let y: Number = 2 in let msg: String = \"ok\" in { print(msg); x + y; };",
            "Program(\n"
            "  ExprStmt(Let(x: Number = Number(1) in Let(y: Number = Number(2) in Let(msg: String = String(\"\"ok\"\") in Block(Call(Identifier(print), [Identifier(msg)]), Binary(Identifier(x), +, Identifier(y)))))))\n"
            ")");

        // Caso 7: asignacion destructiva `:=` a variable ligada por let dentro de un bloque
        // (valor de retorno y reasignacion; print solo comprueba forma del arbol).
        ok &= expect_program_ast(
            "let block with string reassignment via :=",
            grammar,
            ll1_table,
            "let color = \"green\" in { print(color); color := \"blue\"; print(color); };",
            "Program(\n"
            "  ExprStmt(Let(color = String(\"\"green\"\") in Block(Call(Identifier(print), [Identifier(color)]), "
            "Assign(Identifier(color), :=, String(\"\"blue\"\")), Call(Identifier(print), [Identifier(color)]))))\n"
            ")");

        // Caso 8: `:=` asocia a la derecha y tiene menor prioridad que la aritmetica
        // (y := x := 5 + 5  ~  y := (x := (5 + 5)); y := (x := 5) + 1 fuerza otro orden).
        ok &= expect_program_ast(
            "let with chained assignment precedence inside block",
            grammar,
            ll1_table,
            "let x = 0, y = 0 in { y := x := 5 + 5; print(x); print(y); y := (x := 5) + 1; print(x); print(y); };",
            "Program(\n"
            "  ExprStmt(Let(x = Number(0) in Let(y = Number(0) in Block(Assign(Identifier(y), :=, "
            "Assign(Identifier(x), :=, Binary(Number(5), +, Number(5)))), Call(Identifier(print), [Identifier(x)]), "
            "Call(Identifier(print), [Identifier(y)]), Assign(Identifier(y), :=, "
            "Binary(Grouped(Assign(Identifier(x), :=, Number(5))), +, Number(1))), "
            "Call(Identifier(print), [Identifier(x)]), Call(Identifier(print), [Identifier(y)])))))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] let pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
