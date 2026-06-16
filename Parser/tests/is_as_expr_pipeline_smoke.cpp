// Smoke: operador `as` y herencia con `is` en declaraciones de clase.
// Compilar: mingw32-make test_is_as_smoke  o  scripts/run_is_as_smoke.ps1
#include <exception>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>

#include "../ast/cst_to_ast.hpp"
#include "../core/token.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

using parser::TokenList;

std::string join_token_types(const TokenList& tokens) {
    std::ostringstream out;
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) {
            out << ", ";
        }
        out << parser::token_name(tokens[i].type);
    }
    return out.str();
}

TokenList tokenize_source(const std::string& source) {
    std::istringstream input(source);
    return parser::tokenize_stream(input);
}

bool expect_token_types(
    const std::string& name,
    const TokenList& tokens,
    std::initializer_list<parser::TokenType> expected_types) {
    if (tokens.size() != expected_types.size()) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  cantidad esperada: " << expected_types.size() << "\n"
                  << "  cantidad obtenida: " << tokens.size() << "\n"
                  << "  tokens: " << join_token_types(tokens) << "\n";
        return false;
    }

    std::size_t index = 0;
    for (parser::TokenType expected_type : expected_types) {
        if (tokens[index].type != expected_type) {
            std::cerr << "[FAIL] " << name << "\n"
                      << "  token " << index << " esperado: " << parser::token_name(expected_type) << "\n"
                      << "  token " << index << " obtenido: " << parser::token_name(tokens[index].type) << "\n"
                      << "  tokens: " << join_token_types(tokens) << "\n";
            return false;
        }
        ++index;
    }

    std::cout << "[OK] " << name << " -> " << join_token_types(tokens) << "\n";
    return true;
}

bool expect_program_ast(
    const std::string& name,
    const parser::generator::Grammar& grammar,
    const parser::generator::Ll1TableResult& ll1_table,
    const std::string& source,
    const std::string& expected_program) {
    try {
        auto tokens = tokenize_source(source);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        auto program = parser::cst_to_ast(*parse_result.cst_root);
        const auto actual_program = parser::program_to_string(*program);

        if (actual_program != expected_program) {
            std::cerr << "[FAIL] " << name << "\n"
                      << "  esperado: " << expected_program << "\n"
                      << "  obtenido: " << actual_program << "\n";
            return false;
        }

        std::cout << "[OK] " << name << " -> AST esperado\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  excepcion: " << error.what() << "\n";
        return false;
    }
}

bool expect_parse_failure(
    const std::string& name,
    const parser::generator::Grammar& grammar,
    const parser::generator::Ll1TableResult& ll1_table,
    const std::string& source) {
    try {
        auto tokens = tokenize_source(source);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto parse_result = parser.parse();
        parser::cst_to_ast(*parse_result.cst_root);
        std::cerr << "[FAIL] " << name << ": se esperaba error de parseo\n";
        return false;
    } catch (const std::exception&) {
        std::cout << "[OK] " << name << " -> parse error esperado\n";
        return true;
    }
}

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        {
            const auto tokens = tokenize_source("x as Number;");
            ok &= expect_token_types(
                "lexer recognizes as cast",
                tokens,
                {parser::TokenType::IDENTIFIER, parser::TokenType::AS, parser::TokenType::IDENTIFIER,
                 parser::TokenType::SEMICOLON, parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline builds AST for as expression",
                grammar,
                ll1_table,
                "x as Number;",
                "Program(\n"
                "  ExprStmt(As(Identifier(x), Number))\n"
                ")");
        }

        ok &= expect_program_ast(
            "pipeline chains as casts left-associatively",
            grammar,
            ll1_table,
            "(x) as Number as Object;",
            "Program(\n"
            "  ExprStmt(As(As(Grouped(Identifier(x)), Number), Object))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline binds postfix before as at comparison level",
            grammar,
            ll1_table,
            "x.m as Number;",
            "Program(\n"
            "  ExprStmt(As(GetAttr(Identifier(x), m), Number))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline parses class inheritance with is keyword",
            grammar,
            ll1_table,
            "class Child() is Parent() {}",
            "Program(\n"
            "  ClassDecl(Child is Parent {\n"
            "})\n"
            ")");

        ok &= expect_parse_failure(
            "pipeline rejects is as expression operator",
            grammar,
            ll1_table,
            "x is Person;");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] is/as expressions pipeline smoke threw exception: "
                  << error.what() << "\n";
        return 1;
    }
}
