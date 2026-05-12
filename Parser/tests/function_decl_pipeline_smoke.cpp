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

        const char* is_even_expected =
            "Program(\n"
            "  FunctionDecl(isEven(n: Number): Boolean => Binary(Binary(Identifier(n), %, Number(2)), ==, Number(0)))\n"
            "  ExprStmt(Let(x = Number(4) in Call(Identifier(print), [Call(Identifier(isEven), [Identifier(x)])])))\n"
            ")";

        // Forma compacta (una sola expresion terminada en `;`) con flecha `=>` como en Hulk.md.
        ok &= expect_program_ast(
            "compact function with => arrow and typed params",
            grammar,
            ll1_table,
            "function isEven(n:Number):Boolean => n % 2 == 0;\n"
            "let x = 4 in print(isEven(x));",
            is_even_expected);

        // Misma forma compacta con `->` (notacion alternativa del material de curso); el AST es identico.
        ok &= expect_program_ast(
            "compact function with -> arrow (lexeme alias of ARROW)",
            grammar,
            ll1_table,
            "function isEven(n:Number):Boolean -> n % 2 == 0;\n"
            "let x = 4 in print(isEven(x));",
            is_even_expected);

        // Sin anotaciones de tipo en parametros ni tipo de retorno.
        ok &= expect_program_ast(
            "compact function without type annotations",
            grammar,
            ll1_table,
            "function add(x,y) => x + y;\n"
            "print(add(1,2));",
            "Program(\n"
            "  FunctionDecl(add(x, y) => Binary(Identifier(x), +, Identifier(y)))\n"
            "  ExprStmt(Call(Identifier(print), [Call(Identifier(add), [Number(1), Number(2)])]))\n"
            ")");

        // Forma extendida: cuerpo en bloque con varias expresiones; sin `;` despues de la llave de cierre.
        ok &= expect_program_ast(
            "extended function body block then global let calling the function",
            grammar,
            ll1_table,
            "function f(a:Number, b:Number, c:Number):Number { a := b + c; b := c + a; c := a + b; }\n"
            "let a:Number=1, b:Number=2, c:Number=3 in print(f(a,b,c));",
            "Program(\n"
            "  FunctionDecl(f(a: Number, b: Number, c: Number): Number => Block(Binary(Identifier(a), :=, "
            "Binary(Identifier(b), +, Identifier(c))), Binary(Identifier(b), :=, Binary(Identifier(c), +, Identifier(a))), "
            "Binary(Identifier(c), :=, Binary(Identifier(a), +, Identifier(b)))))\n"
            "  ExprStmt(Let(a: Number = Number(1) in Let(b: Number = Number(2) in Let(c: Number = Number(3) in "
            "Call(Identifier(print), [Call(Identifier(f), [Identifier(a), Identifier(b), Identifier(c)])])))))\n"
            ")");

        // Varias funciones compactas antes de la expresion global.
        ok &= expect_program_ast(
            "multiple compact functions then expression statement",
            grammar,
            ll1_table,
            "function double(n:Number):Number => n * 2;\n"
            "function sum(a:Number,b:Number):Number => a + b;\n"
            "print(sum(double(1), double(2)));",
            "Program(\n"
            "  FunctionDecl(double(n: Number): Number => Binary(Identifier(n), *, Number(2)))\n"
            "  FunctionDecl(sum(a: Number, b: Number): Number => Binary(Identifier(a), +, Identifier(b)))\n"
            "  ExprStmt(Call(Identifier(print), [Call(Identifier(sum), [Call(Identifier(double), [Number(1)]), "
            "Call(Identifier(double), [Number(2)])])]))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] function decl pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
