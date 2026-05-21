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

// Este helper deja visibles las clases de token para diagnosticar rapido
// si el fallo viene del lexer antes de culpar al parser.
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

// La prueba recorre el lexer real sobre texto fuente para cubrir el keyword
// `with` y la interaccion con `as`, `else` y `Null`.
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

// Este helper comprueba el pipeline completo hasta el AST final, que es
// justamente el punto que nos interesa validar para esta nueva sintaxis.
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

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        {
            const auto tokens = tokenize_source("with (x as o) o.value else 0;");
            ok &= expect_token_types(
                "lexer recognizes with null-safety tokens",
                tokens,
                {parser::TokenType::WITH, parser::TokenType::LPAREN, parser::TokenType::IDENTIFIER,
                 parser::TokenType::AS, parser::TokenType::IDENTIFIER, parser::TokenType::RPAREN,
                 parser::TokenType::IDENTIFIER, parser::TokenType::DOT, parser::TokenType::IDENTIFIER,
                 parser::TokenType::ELSE, parser::TokenType::NUMBER_LITERAL, parser::TokenType::SEMICOLON,
                 parser::TokenType::EOF_TOKEN});
        }

        ok &= expect_program_ast(
            "pipeline builds AST for with expression with else",
            grammar,
            ll1_table,
            "with (x as o) o.value else 0;",
            "Program(\n"
            "  ExprStmt(With(Identifier(x) as o, GetAttr(Identifier(o), value), Number(0)))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline builds AST for with expression without else",
            grammar,
            ll1_table,
            "with (item as safe) safe.name;",
            "Program(\n"
            "  ExprStmt(With(Identifier(item) as safe, GetAttr(Identifier(safe), name)))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline accepts block body and block else in with expression",
            grammar,
            ll1_table,
            "with (x as o) { o.value; print(o); } else { 0; };",
            "Program(\n"
            "  ExprStmt(With(Identifier(x) as o, Block(GetAttr(Identifier(o), value), Call(Identifier(print), [Identifier(o)])), Block(Number(0))))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline preserves complex source expression in with header",
            grammar,
            ll1_table,
            "with ((fetch(x) @ \"!\") as tmp) tmp.size else Null;",
            "Program(\n"
            "  ExprStmt(With(Grouped(Binary(Call(Identifier(fetch), [Identifier(x)]), @, String(\"\"!\"\"))) as tmp, GetAttr(Identifier(tmp), size), Null))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] with expr pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
