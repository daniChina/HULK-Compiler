#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "../../Parser/ast/cst_to_ast.hpp"
#include "../../Parser/core/parse_error.hpp"
#include "../../Parser/core/token_adapter.hpp"
#include "../../Parser/generator/first_follow.hpp"
#include "../../Parser/generator/grammar_reader.hpp"
#include "../../Parser/generator/ll1_table.hpp"
#include "../../Parser/syntax/ll1_parser.hpp"
#include "../phase2_checker.hpp"

namespace {

bool expect_r1_error_on_source(const std::string& name, const std::string& source) {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        const auto program = parser::cst_to_ast(*parse_result.cst_root);

        semantic::Phase2Analyzer analyzer;
        analyzer.analyze(program.get());
        if (analyzer.hasErrors()) {
            std::cout << "[OK] " << name << " -> " << analyzer.getErrors().front().message << "\n";
            return true;
        }

        std::cerr << "[FAIL] " << name << " — se esperaba error R1 semántico\n";
        return false;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << " — excepcion: " << error.what() << "\n";
        return false;
    }
}

bool expect_r1_ok_on_source(const std::string& name, const std::string& source) {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        const auto program = parser::cst_to_ast(*parse_result.cst_root);

        semantic::Phase2Analyzer analyzer;
        analyzer.analyze(program.get());
        if (!analyzer.hasErrors()) {
            std::cout << "[OK] " << name << " — sin errores R1\n";
            return true;
        }

        analyzer.printErrors();
        std::cerr << "[FAIL] " << name << " — se esperaba programa valido para R1\n";
        return false;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << " — excepcion: " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;
    ok &= expect_r1_error_on_source(
        "R1 duplicate bindings in same let list",
        "let a = 1, a = 2 in print(a);");
    ok &= expect_r1_ok_on_source(
        "R1 shadow nested let is valid",
        "let x = 10 in let x = 20 in print(x);");
    return ok ? 0 : 1;
}
