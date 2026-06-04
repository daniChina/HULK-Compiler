#include <iostream>
#include <sstream>

#include "../../Parser/ast/cst_to_ast.hpp"
#include "../../Parser/core/token_adapter.hpp"
#include "../../Parser/generator/first_follow.hpp"
#include "../../Parser/generator/grammar_reader.hpp"
#include "../../Parser/generator/ll1_table.hpp"
#include "../../Parser/syntax/ll1_parser.hpp"
#include "../evaluator.hpp"

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        std::istringstream input("let x = 1 in print(x);");
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        const auto program = parser::cst_to_ast(*parse_result.cst_root);

        std::ostringstream out;
        eval::Evaluator evaluator(out);
        evaluator.evaluate(program.get());
        if (evaluator.hadError()) {
            std::cerr << "[FAIL] " << evaluator.lastError() << "\n";
            return 1;
        }
        if (out.str() != "1\n") {
            std::cerr << "[FAIL] salida esperada '1\\n', obtuvo: " << out.str() << "\n";
            return 1;
        }
        std::cout << "[OK] eval smoke: let + print\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] excepcion: " << ex.what() << "\n";
        return 1;
    }
}
