#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../Parser/ast/cst_to_ast.hpp"
#include "../Parser/core/parse_error.hpp"
#include "../Parser/core/token_adapter.hpp"
#include "../SymbolTable/decl_collector.hpp"
#include "../SymbolTable/symbol_table.hpp"
#include "../Types/type_info.hpp"
#include "../Parser/generator/first_follow.hpp"
#include "../Parser/generator/grammar_reader.hpp"
#include "../Parser/generator/ll1_table.hpp"
#include "../Parser/syntax/ll1_parser.hpp"
#include "../SemanticCheck/phase2_checker.hpp"
#include "../Evaluator/evaluator.hpp"
#include "matcom_diagnostic.hpp"
#include "output_gen.hpp"
#include "pipeline.hpp"

namespace {

struct Options {
    bool print_tokens = false;
    bool print_cst = false;
    bool print_ast = false;
    bool print_symbols = false;
    bool run_semantic = false;
    bool run_interpret = false;
    bool matcom_mode = false;
    std::string input_path;
};

void print_usage(const char* program_name) {
    std::cerr
        << "Uso: " << program_name << " <archivo.hulk>\n"
        << "     " << program_name
        << " [--tokens] [--cst] [--ast] [--symbols] [--semantic] [--interpret] [archivo.hulk]\n"
        << "  Modo matcom (sin flags): compila a ./output\n"
        << "  Modo desarrollo: flags de depuracion; --interpret ejecuta el evaluador\n";
}

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
        if (arg == "--symbols") {
            options.print_symbols = true;
            continue;
        }
        if (arg == "--semantic") {
            options.run_semantic = true;
            continue;
        }
        if (arg == "--interpret") {
            options.run_interpret = true;
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

    const bool dev_mode = options.print_tokens || options.print_cst || options.print_ast ||
                          options.print_symbols || options.run_semantic || options.run_interpret;
    options.matcom_mode = !dev_mode && !options.input_path.empty();
    return options;
}

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error("No se pudo abrir el archivo: " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

void print_tokens(const parser::TokenList& tokens) {
    for (const auto& token : tokens) {
        std::cout << token_name(token.type)
                  << " \"" << token.lexeme << "\""
                  << " @ (" << token.line << ", " << token.col << ")\n";
    }
}

int run_matcom(const std::string& input_path) {
    const std::string source = read_file(input_path);
    const auto compiled = hulk::compile_program(source);
    if (compiled.diagnostic.exit_code != 0) {
        for (const auto& line : compiled.diagnostic.lines) {
            std::cerr << line << '\n';
        }
        return compiled.diagnostic.exit_code;
    }

    std::string build_error;
    if (!hulk::build_output_executable(source, compiled.program.get(), &build_error)) {
        const std::string message =
            build_error.empty() ? "code generation failed" : build_error;
        std::cerr << hulk::format_semantic(0, 0, message) << '\n';
        return 3;
    }

    return 0;
}

int run_development(const Options& options) {
    const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
    const auto first_follow = parser::generator::compute_first_follow(grammar);
    const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

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

    parser::Ll1Parser parser_engine(std::move(tokens), grammar, ll1_table.table);
    auto parse_result = parser_engine.parse();

    const bool quiet_pipeline = options.run_interpret;

    if (!quiet_pipeline) {
        std::cout << "Parse OK\n";
    }

    if (options.print_cst && parse_result.cst_root) {
        std::cout << "== CST ==\n";
        std::cout << parser::cst_to_string(*parse_result.cst_root);
    }

    if (parse_result.cst_root) {
        auto ast = parser::cst_to_ast(*parse_result.cst_root);

        if (options.print_ast) {
            std::cout << "== AST ==\n";
            std::cout << parser::program_to_string(*ast) << "\n";
        }

        if (options.print_symbols) {
            SymbolTable symbol_table;
            TypeInfo::setSymbolTable(&symbol_table);
            const auto decl_stats = collectTopLevelDeclarations(*ast, symbol_table);
            std::cout << "== Symbols ==\n"
                      << "  functions registered: " << decl_stats.functions_registered << "\n"
                      << "  types registered: " << decl_stats.types_registered << "\n"
                      << "  stmts skipped (non-decl): " << decl_stats.skipped_stmts << "\n";
        }

        if (options.run_semantic) {
            semantic::Phase2Analyzer analyzer;
            analyzer.analyze(ast.get());
            if (analyzer.hasErrors()) {
                std::cerr << "== Semantic errors ==\n";
                analyzer.printErrors();
                return 1;
            }
            if (!quiet_pipeline) {
                std::cout << "Semantic OK\n";
            }
        }

        if (options.run_interpret) {
            eval::Evaluator evaluator;
            evaluator.evaluate(ast.get());
            if (evaluator.hadError()) {
                std::cerr << evaluator.lastError() << "\n";
                return 1;
            }
        }

    }

    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 2;
    }

    try {
        const auto options = parse_options(argc, argv);
        if (options.matcom_mode) {
            return run_matcom(options.input_path);
        }
        return run_development(options);
    } catch (const parser::ParseError& error) {
        std::cerr << hulk::format_syntactic(error.found().line, error.found().col,
                                            error.user_message())
                  << '\n';
        return 2;
    } catch (const std::exception& error) {
        std::cerr << hulk::format_syntactic(0, 0, error.what()) << '\n';
        return 2;
    }
}
