#include <exception>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../Parser/ast/cst_to_ast.hpp"
#include "../Parser/core/token_adapter.hpp"
#include "../Parser/generator/first_follow.hpp"
#include "../Parser/generator/grammar_reader.hpp"
#include "../Parser/generator/ll1_table.hpp"
#include "../Parser/syntax/ll1_parser.hpp"

namespace {

struct Options {
    bool print_tokens = false;
    bool print_cst = false;
    bool print_ast = false;
    std::string input_path;
};

void print_usage(const char* program_name) {
    std::cerr
        << "Uso: " << program_name << " [--tokens] [--cst] [--ast] [archivo.hulk]\n"
        << "  Si no se pasa archivo, lee desde stdin.\n";
}

// Parser minimo de argumentos para no depender de ninguna libreria externa.
Options parse_options(int argc, char* argv[]) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "--tokens") {
            options.print_tokens = true;
            continue;
        }
        if (arg == "--cst") {
            options.print_cst = true;
            continue;
        }
        if (arg == "--ast") {
            options.print_ast = true;
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }

        if (!options.input_path.empty()) {
            throw std::runtime_error("Solo se admite un archivo de entrada");
        }

        options.input_path = arg;
    }

    return options;
}

void print_tokens(const parser::TokenList& tokens) {
    for (const auto& token : tokens) {
        std::cout << token_name(token.type)
                  << " \"" << token.lexeme << "\""
                  << " @ (" << token.line << ", " << token.col << ")\n";
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    try {
        const auto options = parse_options(argc, argv);
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        // Antes de parsear entrada real, se valida que la gramatica actual no tenga conflictos LL(1).
        if (!ll1_table.conflicts.empty()) {
            std::cerr << "La gramatica actual no es LL(1). Conflictos detectados:\n";
            for (const auto& conflict : ll1_table.conflicts) {
                std::cerr << "  - " << parser::generator::conflict_to_string(conflict) << "\n";
            }
            return 1;
        }

        parser::TokenList tokens;

        if (!options.input_path.empty()) {
            tokens = parser::tokenize_file(options.input_path);
        } else {
            tokens = parser::tokenize_stream(std::cin);
        }

        if (options.print_tokens) {
            std::cout << "== Tokens ==\n";
            print_tokens(tokens);
        }

        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        auto parse_result = parser.parse();

        std::cout << "Parse OK\n";

        if (options.print_cst && parse_result.cst_root) {
            std::cout << "== CST ==\n";
            std::cout << parser::cst_to_string(*parse_result.cst_root);
        }

        if (options.print_ast && parse_result.cst_root) {
            auto ast = parser::cst_to_ast(*parse_result.cst_root);
            std::cout << "== AST ==\n";
            std::cout << parser::expr_to_string(*ast) << "\n";
        }

        return 0;
    } catch (const parser::ParseError& error) {
        std::cerr << error.what() << "\n";
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
