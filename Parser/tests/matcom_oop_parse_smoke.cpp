#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../ast/cst_to_ast.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    std::cout << "[OK] " << message << "\n";
    return true;
}

bool parse_file_ok(const parser::generator::Grammar& grammar,
                   const parser::generator::Ll1Table& table,
                   const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        std::cerr << "[FAIL] no se pudo abrir " << path << "\n";
        return false;
    }
    auto tokens = parser::tokenize_stream(input);
    parser::Ll1Parser parser_engine(std::move(tokens), grammar, table);
    const auto result = parser_engine.parse();
    parser::cst_to_ast(*result.cst_root);
    return true;
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        if (!ll1_table.conflicts.empty()) {
            std::cerr << "[FAIL] gramatica con conflictos LL(1)\n";
            return 1;
        }

        const std::vector<std::string> fixtures = {
            "tests/matcom/oop/basic_class.hulk",
            "tests/matcom/oop/class_interaction.hulk",
            "tests/matcom/oop/constructor_expr.hulk",
            "tests/matcom/oop/inheritance.hulk",
            "tests/matcom/oop/method_override.hulk",
            "tests/matcom/oop/multilevel.hulk",
            "tests/matcom/oop/mutation.hulk",
            "tests/matcom/oop/polymorphism.hulk",
            "tests/matcom/oop/self_method.hulk",
            "tests/matcom/oop/vector_math.hulk",
        };

        bool ok = true;
        for (const auto& path : fixtures) {
            ok &= expect(parse_file_ok(grammar, ll1_table.table, path), "parse OK: " + path);
        }

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] matcom oop parse smoke: " << error.what() << "\n";
        return 1;
    }
}
