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

// Estas pruebas fijan justo la restricción de 8.2.11: ciertas expresiones
// de baja prioridad solo pueden aparecer dentro de otras si van parentizadas.
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

        ok &= expect_parse_error(
            "rejects unparenthesized let inside addition",
            grammar,
            ll1_table,
            "let x:Number=5 in x + let y:Number=8 in y;");

        ok &= expect_parse_error(
            "rejects unparenthesized if inside addition",
            grammar,
            ll1_table,
            "x + if (a) b else c;");

        ok &= expect_parse_error(
            "rejects unparenthesized while inside addition",
            grammar,
            ll1_table,
            "x + while (a) y else z;");

        ok &= expect_parse_error(
            "rejects unparenthesized with inside addition",
            grammar,
            ll1_table,
            "x + with (y as z) z else 0;");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] precedence invalid smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
