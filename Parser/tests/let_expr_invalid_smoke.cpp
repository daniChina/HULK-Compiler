#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "../ast/cst_to_ast.hpp"
#include "../core/parse_error.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

// Ejecuta el pipeline real y exige que termine en error sintactico.
bool expect_parse_error(
    const std::string& name,
    const parser::generator::Grammar& grammar,
    const parser::generator::Ll1TableResult& ll1_table,
    const std::string& source) {
    try {
        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto result = parser.parse();
        (void)result;

        std::cerr << "[FAIL] " << name << "\n"
                  << "  se esperaba ParseError y el programa fue aceptado\n";
        return false;
    } catch (const parser::ParseError& error) {
        std::cout << "[OK] " << name << " -> " << error.what() << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  excepcion inesperada: " << error.what() << "\n";
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

        // let sin identificador para el binding.
        ok &= expect_parse_error(
            "rejects let without variable name",
            grammar,
            ll1_table,
            "let = 1 in x;");

        // let sin inicializador.
        ok &= expect_parse_error(
            "rejects let without initializer",
            grammar,
            ll1_table,
            "let x = in x;");

        // let sin palabra clave in.
        ok &= expect_parse_error(
            "rejects let without in keyword",
            grammar,
            ll1_table,
            "let x = 1 x + 1;");

        // lista multiple con coma final colgante.
        ok &= expect_parse_error(
            "rejects trailing comma in let bindings",
            grammar,
            ll1_table,
            "let x = 1, in x;");

        // variante tipada sin nombre de tipo.
        ok &= expect_parse_error(
            "rejects let type annotation without type name",
            grammar,
            ll1_table,
            "let msg: = \"hola\" in msg;");

        // un bloque no puede aparecer como inicializador de let.
        ok &= expect_parse_error(
            "rejects block as let initializer",
            grammar,
            ll1_table,
            "let x = { 0; 1; } in print(x);");

        // un bloque ya no es una expresion global por si mismo.
        ok &= expect_parse_error(
            "rejects standalone block expression statement",
            grammar,
            ll1_table,
            "{ 1; 2; };");

        // tampoco se puede usar un bloque dentro de una suma.
        ok &= expect_parse_error(
            "rejects block used inside arithmetic expression",
            grammar,
            ll1_table,
            "{ 1; 2; } + 3;");

        // ni como argumento de llamada, porque no es Expr general.
        ok &= expect_parse_error(
            "rejects block used as call argument",
            grammar,
            ll1_table,
            "print({ 1; 2; });");

        // el contexto permitido en esta fase es el body del let, no un subtermino
        // arbitrario del body.
        ok &= expect_parse_error(
            "rejects block on rhs inside let body expression",
            grammar,
            ll1_table,
            "let x = 1 in x + { 2; 3; };");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] let invalid smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
