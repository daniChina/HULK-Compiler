#include <iostream>
#include <sstream>
#include <string>

#include "../../Parser/ast/cst_to_ast.hpp"
#include "../../Parser/core/token_adapter.hpp"
#include "../../Parser/generator/first_follow.hpp"
#include "../../Parser/generator/grammar_reader.hpp"
#include "../../Parser/generator/ll1_table.hpp"
#include "../../Parser/syntax/ll1_parser.hpp"
#include "../evaluator.hpp"

namespace {

bool runProgram(const std::string& source, const std::string& expected_stdout, const char* label) {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        const auto program = parser::cst_to_ast(*parse_result.cst_root);

        std::ostringstream out;
        eval::Evaluator evaluator(out);
        evaluator.evaluate(program.get());
        if (evaluator.hadError()) {
            std::cerr << "[FAIL] " << label << ": " << evaluator.lastError() << "\n";
            return false;
        }
        if (out.str() != expected_stdout) {
            std::cerr << "[FAIL] " << label << ": salida esperada '" << expected_stdout << "', obtuvo '"
                      << out.str() << "'\n";
            return false;
        }
        std::cout << "[OK] " << label << "\n";
        return true;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << label << ": excepcion: " << ex.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    bool ok = true;

    ok &= runProgram("let x = 1 in print(x);", "1\n", "eval smoke: let + print");

    ok &= runProgram("function f(x) => x + 1; print(f(2));", "3\n", "B9: function call");

    ok &= runProgram(
        "function caller() => callee(); function callee() => print(1); caller();",
        "1\n",
        "B8: forward reference via dual pass");

    ok &= runProgram(
        "function fact(n) => if n <= 1 then 1 else n * fact(n - 1); print(fact(5));",
        "120\n",
        "B9: recursion");

    ok &= runProgram(
        "function f(x) => x; function f(x, y) => x + y; print(f(3, 4));",
        "7\n",
        "B9: overload by arity");

    return ok ? 0 : 1;
}
