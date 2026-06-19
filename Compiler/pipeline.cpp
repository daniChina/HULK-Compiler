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

CompileDiagnostic fail_lexical(int line, int col, const std::string& message) {
    CompileDiagnostic result;
    result.phase = CompilePhase::Lexical;
    result.exit_code = 1;
    result.lines.push_back(format_lexical(line, col, message));
    return result;
}

CompileDiagnostic fail_syntactic(int line, int col, const std::string& message) {
    CompileDiagnostic result;
    result.phase = CompilePhase::Syntactic;
    result.exit_code = 2;
    result.lines.push_back(format_syntactic(line, col, message));
    return result;
}

CompileDiagnostic fail_semantic(const std::vector<SemanticError>& errors) {
    CompileDiagnostic result;
    result.phase = CompilePhase::Semantic;
    result.exit_code = 3;
    std::vector<std::string> seen;
    for (const auto& error : errors) {
        const int line = error.line > 0 ? error.line : 0;
        const int col = error.column > 0 ? error.column : 0;
        const std::string formatted = format_semantic(line, col, error.message);
        if (std::find(seen.begin(), seen.end(), formatted) != seen.end()) {
            continue;
        }
        seen.push_back(formatted);
        result.lines.push_back(formatted);
    }
    if (result.lines.empty()) {
        result.lines.push_back(format_semantic(0, 0, "semantic analysis failed"));
    }
    return result;
}

}  // namespace

CompileDiagnostic compile_source(const std::string& source, const std::string& grammar_path) {
    return compile_program(source, grammar_path).diagnostic;
}

CompiledProgram compile_program(const std::string& source, const std::string& grammar_path) {
    CompiledProgram result;

    const auto grammar = parser::generator::read_grammar_file(grammar_path);
    const auto first_follow = parser::generator::compute_first_follow(grammar);
    const auto& table = ll1_table_cached(grammar, first_follow);

    parser::TokenList tokens = tokenize_source_string(source);
    for (const auto& token : tokens) {
        if (token.type == parser::TokenType::UNKNOWN) {
            const std::string ch = token.lexeme.empty() ? "?" : token.lexeme;
            result.diagnostic = fail_lexical(token.line, token.col,
                                             "unexpected character '" + ch + "'");
            return result;
        }
    }

    try {
        parser::Ll1Parser parser_engine(std::move(tokens), grammar, table);
        const auto parse_result = parser_engine.parse();
        auto program = parser::cst_to_ast(*parse_result.cst_root);

        semantic::Phase2Analyzer analyzer;
        analyzer.analyze(program.get());
        if (analyzer.hasErrors()) {
            result.diagnostic = fail_semantic(analyzer.getErrors());
            return result;
        }

        result.program = std::move(program);
        result.diagnostic.phase = CompilePhase::Ok;
        result.diagnostic.exit_code = 0;
        return result;
    } catch (const parser::ParseError& error) {
        const auto& token = error.found();
        result.diagnostic = fail_syntactic(token.line, token.col, error.user_message());
        return result;
    } catch (const std::exception& error) {
        result.diagnostic = fail_syntactic(0, 0, error.what());
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
