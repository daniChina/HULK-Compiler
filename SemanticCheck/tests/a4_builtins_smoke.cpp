#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "../../Parser/ast/cst_to_ast.hpp"
#include "../../Parser/core/token_adapter.hpp"
#include "../../Parser/generator/first_follow.hpp"
#include "../../Parser/generator/grammar_reader.hpp"
#include "../../Parser/generator/ll1_table.hpp"
#include "../../Parser/syntax/ll1_parser.hpp"
#include "../../SymbolTable/symbol_table.hpp"
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

bool expect_table_builtins() {
    SymbolTable table;
    bool ok = true;
    if (!table.isBuiltinVariable("PI") || !table.isBuiltinVariable("E")) {
        std::cerr << "[FAIL] SymbolTable no marca PI/E como builtin\n";
        ok = false;
    } else {
        std::cout << "[OK] SymbolTable: PI/E registrados como builtin\n";
    }
    if (!table.isBuiltinFunction("print") || !table.isBuiltinFunction("sin")) {
        std::cerr << "[FAIL] SymbolTable no marca print/sin como builtin\n";
        ok = false;
    } else {
        std::cout << "[OK] SymbolTable: print/sin registrados como builtin\n";
    }
    if (!table.lookupVariable("PI") || !table.lookupFunction("print", 1)) {
        std::cerr << "[FAIL] lookupVariable/lookupFunction no resuelven builtins\n";
        ok = false;
    } else {
        std::cout << "[OK] resolución #11: lookupVariable(PI), lookupFunction(print,1)\n";
    }
    return ok;
}

bool expect_error(const std::string& name, const std::string& source) {
    try {
        semantic::Phase2Analyzer analyzer;
        run_semantic(source, analyzer);
        if (!analyzer.hasErrors()) {
            std::cerr << "[FAIL] " << name << " — se esperaba error\n";
            return false;
        }
        std::cout << "[OK] " << name << " -> " << analyzer.getErrors().front().message << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << " — " << error.what() << "\n";
        return false;
    }
}

bool expect_ok(const std::string& name, const std::string& source) {
    try {
        semantic::Phase2Analyzer analyzer;
        run_semantic(source, analyzer);
        if (analyzer.hasErrors()) {
            analyzer.printErrors();
            std::cerr << "[FAIL] " << name << "\n";
            return false;
        }
        std::cout << "[OK] " << name << " — sin errores\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << " — " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;
    ok &= expect_table_builtins();
    ok &= expect_error("A4 let PI", "let PI = 1 in print(PI);");
    ok &= expect_error("A4 redeclare sin", "function sin(x) => x;\nprint(sin(1));");
    ok &= expect_ok("A4 use builtins via lookup", "print(PI);\nprint(sin(1));");
    return ok ? 0 : 1;
}
