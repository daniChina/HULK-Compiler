#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "../core/parse_error.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

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
            "rejects if without opening paren after if",
            grammar,
            ll1_table,
            "if true) 1;");

        ok &= expect_parse_error(
            "rejects if with missing then body before else",
            grammar,
            ll1_table,
            "if (true) else 1;");

        ok &= expect_parse_error(
            "rejects elif without parenthesized condition",
            grammar,
            ll1_table,
            "if (true) 1 elif 2;");

        ok &= expect_parse_error(
            "rejects else without body",
            grammar,
            ll1_table,
            "if (true) 1 else;");

        ok &= expect_parse_error(
            "rejects elseif keyword (must be elif)",
            grammar,
            ll1_table,
            "if (true) 1 elseif (false) 2;");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] if expr invalid smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
