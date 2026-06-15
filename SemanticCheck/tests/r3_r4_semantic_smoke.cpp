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

    ok &= expect_error("R3 duplicate params", "function bad(x, x) => x;\nprint(bad(1));",
                       ErrorType::REDEFINED_VARIABLE);
    ok &= expect_error("R3 same arity overload",
                       "function suma(a, b) => a + b;\nfunction suma(x, y) => x + y;\nprint(suma(1, 2));",
                       ErrorType::REDEFINED_FUNCTION);
    ok &= expect_ok("R4 forward reference in let",
                    "let r = f(1) in print(r);\nfunction f(n) => n;");
    ok &= expect_ok("R4 mutual forward ( collectFunctions)",
                    "function a() => b();\nfunction b() => 1;\nprint(a());");
    ok &= expect_ok("R4 call before textual decl",
                    "print(h(1));\nfunction h(x) => x;");

    ok &= expect_ok("R3 distinct arity overload",
                    "function suma(a, b) => a + b;\nfunction suma(a, b, c) => a + b + c;\n"
                    "print(suma(1, 2));\nprint(suma(1, 2, 3));");
    ok &= expect_ok("R4 declare b then a",
                    "function b() => 1;\nfunction a() => b();\nprint(a());");
    ok &= expect_ok("R4 builtin print", "print(\"ok\");");

    return ok ? 0 : 1;
}
