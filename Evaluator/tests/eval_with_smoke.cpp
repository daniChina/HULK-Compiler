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

    ok &= runProgram("print(with (1 as a) a);", "1\n", "B7: with binds alias");
    ok &= runProgram("print(with (Null as x) x else 42);", "42\n", "B7: with null else");
    ok &= runProgram("with (1 as a) print(a);", "1\n", "B7: with stmt body");

    return ok ? 0 : 1;
}
