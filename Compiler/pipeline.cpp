#include "pipeline.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

#include "../Evaluator/evaluator.hpp"
#include "../Parser/core/parse_error.hpp"
#include "../Parser/core/token_adapter.hpp"
#include "../Parser/generator/first_follow.hpp"
#include "../Parser/generator/grammar_reader.hpp"
#include "../Parser/generator/ll1_table.hpp"
#include "../Parser/syntax/ll1_parser.hpp"
#include "../SemanticCheck/error.hpp"
#include "../SemanticCheck/phase2_checker.hpp"
#include "diagnostic.hpp"
#include "matcom_diagnostic.hpp"

namespace hulk {
namespace {

const parser::generator::Ll1Table& ll1_table_cached(
    const parser::generator::Grammar& grammar,
    const parser::generator::FirstFollowResult& first_follow) {
    static std::unique_ptr<parser::generator::Ll1Table> cached_table;
    if (!cached_table) {
        const auto built = parser::generator::build_ll1_table(grammar, first_follow);
        cached_table = std::make_unique<parser::generator::Ll1Table>(built.table);
    }
    return *cached_table;
}

parser::TokenList tokenize_source_string(const std::string& source) {
    std::istringstream input(source);
    return parser::tokenize_stream(input);
}

CompileDiagnostic fail_lexical(int line, int col, const std::string& message, bool all_errors) {
    CompileDiagnostic result;
  std::vector<Diagnostic> items = {
        Diagnostic{line, col, DiagnosticKind::Lexical, message},
    };
    finalize_compile_diagnostic(result.phase, result.exit_code, items, result.lines, all_errors);
    result.items = std::move(items);
    return result;
}

CompileDiagnostic fail_syntactic(int line, int col, const std::string& message, bool all_errors) {
    CompileDiagnostic result;
    std::vector<Diagnostic> items = {
        Diagnostic{line, col, DiagnosticKind::Syntactic, message},
    };
    finalize_compile_diagnostic(result.phase, result.exit_code, items, result.lines, all_errors);
    result.items = std::move(items);
    return result;
}

CompileDiagnostic fail_semantic(const std::vector<SemanticError>& errors, bool all_errors) {
    CompileDiagnostic result;
    std::vector<Diagnostic> items;
    for (const auto& error : errors) {
        const int line = error.line > 0 ? error.line : 0;
        const int col = error.column > 0 ? error.column : 0;
        items.push_back(Diagnostic{line, col, DiagnosticKind::Semantic, error.message});
    }
    if (items.empty()) {
        items.push_back(Diagnostic{0, 0, DiagnosticKind::Semantic, "semantic analysis failed"});
    }
    finalize_compile_diagnostic(result.phase, result.exit_code, items, result.lines, all_errors);
    result.items = std::move(items);
    return result;
}

}  // namespace

CompileDiagnostic compile_source(const std::string& source, const std::string& grammar_path) {
    return compile_program(source, grammar_path).diagnostic;
}

CompiledProgram compile_program(const std::string& source,
                                const std::string& grammar_path,
                                CompileOptions options) {
    CompiledProgram result;

    const auto grammar = parser::generator::read_grammar_file(grammar_path);
    const auto first_follow = parser::generator::compute_first_follow(grammar);
    const auto& table = ll1_table_cached(grammar, first_follow);

    parser::TokenList tokens = tokenize_source_string(source);
    for (const auto& token : tokens) {
        if (token.type == parser::TokenType::UNKNOWN) {
            const std::string message =
                token.lexeme == "unclosed string"
                    ? "cadena sin cerrar"
                    : "unexpected character '" +
                          (token.lexeme.empty() ? "?" : token.lexeme) + "'";
            result.diagnostic = fail_lexical(token.line, token.col, message, options.all_errors);
            return result;
        }
        if (token.type == parser::TokenType::STRING_LITERAL && !token.lexeme.empty() &&
            token.lexeme.front() == '"' && token.lexeme.back() != '"') {
            result.diagnostic = fail_lexical(token.line, token.col,
                                             "cadena sin cerrar",
                                             options.all_errors);
            return result;
        }
    }

    try {
        const auto& stmt_first = first_follow.first_sets.at("Stmt");
        parser::Ll1Parser parser_engine(std::move(tokens), grammar, table, &stmt_first);

        parser::Ll1ParseResult parse_result;
        std::vector<Diagnostic> syntactic_items;

        if (options.all_errors) {
            parse_result = parser_engine.parse_with_recovery();
            for (const auto& error : parse_result.errors) {
                syntactic_items.push_back(
                    Diagnostic{error.line, error.col, DiagnosticKind::Syntactic, error.message});
            }
        } else {
            parse_result = parser_engine.parse();
        }

        parser::ProgramPtr program;
        try {
            program = parser::cst_to_ast(*parse_result.cst_root);
        } catch (const std::exception& conversion_error) {
            if (!options.all_errors) {
                result.diagnostic = fail_syntactic(0, 0, conversion_error.what(), false);
                return result;
            }
            syntactic_items.push_back(
                Diagnostic{0, 0, DiagnosticKind::Syntactic, conversion_error.what()});
        }

        if (program) {
            semantic::Phase2Analyzer analyzer;
            analyzer.analyze(program.get());
            if (analyzer.hasErrors()) {
                const auto semantic_diag = fail_semantic(analyzer.getErrors(), options.all_errors);
                if (options.all_errors) {
                    result.diagnostic.items =
                        merge_diagnostics(std::move(syntactic_items), semantic_diag.items);
                    finalize_compile_diagnostic(result.diagnostic.phase,
                                                result.diagnostic.exit_code,
                                                result.diagnostic.items,
                                                result.diagnostic.lines,
                                                true);
                    return result;
                }
                result.diagnostic = semantic_diag;
                return result;
            }
        }

        if (!syntactic_items.empty()) {
            result.diagnostic.items = std::move(syntactic_items);
            finalize_compile_diagnostic(result.diagnostic.phase,
                                        result.diagnostic.exit_code,
                                        result.diagnostic.items,
                                        result.diagnostic.lines,
                                        options.all_errors);
            return result;
        }

        if (!program) {
            result.diagnostic = fail_syntactic(0, 0, "syntax analysis failed", options.all_errors);
            return result;
        }

        result.program = std::move(program);
        result.diagnostic.phase = CompilePhase::Ok;
        result.diagnostic.exit_code = 0;
        return result;
    } catch (const parser::ParseError& error) {
        const auto& token = error.found();
        result.diagnostic = fail_syntactic(token.line, token.col, error.user_message(),
                                           options.all_errors);
        return result;
    } catch (const std::exception& error) {
        result.diagnostic = fail_syntactic(0, 0, error.what(), options.all_errors);
        return result;
    }
}


int run_interpreted(const parser::Program& program, std::ostream& out, std::ostream& err) {
    eval::Evaluator evaluator(out);
    evaluator.evaluate(const_cast<parser::Program*>(&program));
    if (evaluator.hadError()) {
        err << evaluator.lastError() << '\n';
        return 1;
    }
    return 0;
}

int run_embedded_program(const std::string& source, std::ostream& out, std::ostream& err) {
    const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
    const auto first_follow = parser::generator::compute_first_follow(grammar);
    const auto& table = ll1_table_cached(grammar, first_follow);

    try {
        auto tokens = tokenize_source_string(source);
        parser::Ll1Parser parser_engine(std::move(tokens), grammar, table);
        const auto parse_result = parser_engine.parse();
        auto program = parser::cst_to_ast(*parse_result.cst_root);

        semantic::Phase2Analyzer analyzer;
        analyzer.analyze(program.get());
        if (analyzer.hasErrors()) {
            err << "embedded program failed semantic checks\n";
            return 1;
        }

        return run_interpreted(*program, out, err);
    } catch (const std::exception& error) {
        err << error.what() << '\n';
        return 1;
    }
}

}  // namespace hulk
