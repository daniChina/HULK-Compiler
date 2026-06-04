#include <exception>
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

bool run_semantic(const std::string& source, semantic::Phase2Analyzer& analyzer) {
    const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
    const auto first_follow = parser::generator::compute_first_follow(grammar);
    const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

    std::istringstream input(source);
    auto tokens = parser::tokenize_stream(input);
    parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
    const auto parse_result = parser.parse();
    const auto program = parser::cst_to_ast(*parse_result.cst_root);
    analyzer.analyze(program.get());
    return true;
}

bool expect_error(const std::string& name, const std::string& source, ErrorType expected) {
    try {
        semantic::Phase2Analyzer analyzer;
        run_semantic(source, analyzer);
        if (!analyzer.hasErrors()) {
            std::cerr << "[FAIL] " << name << " — se esperaba error\n";
            return false;
        }
        const auto& err = analyzer.getErrors().front();
        if (err.type != expected) {
            std::cerr << "[FAIL] " << name << " — tipo " << static_cast<int>(err.type)
                      << " vs esperado " << static_cast<int>(expected) << "\n";
            return false;
        }
        std::cout << "[OK] " << name << " -> " << err.message << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << " — excepcion: " << error.what() << "\n";
        return false;
    }
}

bool expect_ok(const std::string& name, const std::string& source) {
    try {
        semantic::Phase2Analyzer analyzer;
        run_semantic(source, analyzer);
        if (analyzer.hasErrors()) {
            analyzer.printErrors();
            std::cerr << "[FAIL] " << name << " — se esperaba programa valido\n";
            return false;
        }
        std::cout << "[OK] " << name << " — sin errores\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << " — excepcion: " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    ok &= expect_error("R2 use before let", "let y = x in let x = 1 in print(y);",
                       ErrorType::UNDEFINED_VARIABLE);
    ok &= expect_error("R2 free identifier", "print(z);", ErrorType::UNDEFINED_VARIABLE);
    ok &= expect_error("R2 var inside function used outside",
                       "function g() => let a = 1 in a;\nprint(a);",
                       ErrorType::UNDEFINED_VARIABLE);
    ok &= expect_error("R2 self in initializer", "let x = x in print(x);",
                       ErrorType::UNDEFINED_VARIABLE);

    ok &= expect_ok("R2 let then use", "let x = 1 in print(x);");
    ok &= expect_ok("R2 binding uses previous", "let a = 1, b = a + 1 in print(b);");
    ok &= expect_ok("R2 builtin PI", "print(PI);");
    ok &= expect_ok("R2 function param in body", "function f(x) => print(x);\nprint(f(2));");
    ok &= expect_ok("R2 outer passed to global function",
                    "function inc(x) => x + 1;\nlet outer = 1 in print(inc(outer));");
    ok &= expect_ok("R2 assign in inner let", "let x = 1 in let x := 2 in print(x);");

    return ok ? 0 : 1;
}
