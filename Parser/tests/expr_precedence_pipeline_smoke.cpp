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

// Muestra la secuencia de tipos para que los fallos de lexer se entiendan
// rápido antes de mirar la gramática o la conversión a AST.
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

// Recorre el pipeline completo para fijar la precedencia observada en el AST.
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
            const auto tokens = tokenize_source("let x:Number=5 in let y:Number=8 in x+y;");
            ok &= expect_token_types(
                "lexer recognizes precedence example with typed nested lets",
                tokens,
                {parser::TokenType::LET, parser::TokenType::IDENTIFIER, parser::TokenType::COLON,
                 parser::TokenType::IDENTIFIER, parser::TokenType::EQUAL, parser::TokenType::NUMBER_LITERAL,
                 parser::TokenType::IN, parser::TokenType::LET, parser::TokenType::IDENTIFIER,
                 parser::TokenType::COLON, parser::TokenType::IDENTIFIER, parser::TokenType::EQUAL,
                 parser::TokenType::NUMBER_LITERAL, parser::TokenType::IN, parser::TokenType::IDENTIFIER,
                 parser::TokenType::PLUS, parser::TokenType::IDENTIFIER, parser::TokenType::SEMICOLON,
                 parser::TokenType::EOF_TOKEN});
        }

        ok &= expect_program_ast(
            "let associates to the right at lowest precedence",
            grammar,
            ll1_table,
            "let x:Number=5 in let y:Number=8 in x+y;",
            "Program(\n"
            "  ExprStmt(Let(x: Number = Number(5) in Let(y: Number = Number(8) in Binary(Identifier(x), +, Identifier(y)))))\n"
            ")");

        ok &= expect_program_ast(
            "parenthesized let can appear inside arithmetic expression",
            grammar,
            ll1_table,
            "let x:Number=5 in x + (let y:Number=8 in y);",
            "Program(\n"
            "  ExprStmt(Let(x: Number = Number(5) in Binary(Identifier(x), +, Grouped(Let(y: Number = Number(8) in Identifier(y))))))\n"
            ")");

        ok &= expect_program_ast(
            "else associates to the nearest if",
            grammar,
            ll1_table,
            "if (a) if (b) y else z;",
            "Program(\n"
            "  ExprStmt(If(Identifier(a), If(Identifier(b), Identifier(y), Identifier(z))))\n"
            ")");

        ok &= expect_program_ast(
            "else associates to the nearest while",
            grammar,
            ll1_table,
            "while (a) while (b) y else z;",
            "Program(\n"
            "  ExprStmt(While(Identifier(a), While(Identifier(b), Identifier(y), Identifier(z))))\n"
            ")");

        ok &= expect_program_ast(
            "with associates to the right at lowest precedence",
            grammar,
            ll1_table,
            "with (x as o) with (y as p) p.value else o.value;",
            "Program(\n"
            "  ExprStmt(With(Identifier(x) as o, With(Identifier(y) as p, GetAttr(Identifier(p), value), GetAttr(Identifier(o), value))))\n"
            ")");

        ok &= expect_program_ast(
            "call has higher precedence than addition",
            grammar,
            ll1_table,
            "f(x) + 1;",
            "Program(\n"
            "  ExprStmt(Binary(Call(Identifier(f), [Identifier(x)]), +, Number(1)))\n"
            ")");

        ok &= expect_program_ast(
            "attribute access has higher precedence than multiplication",
            grammar,
            ll1_table,
            "obj.value * 2;",
            "Program(\n"
            "  ExprStmt(Binary(GetAttr(Identifier(obj), value), *, Number(2)))\n"
            ")");

        ok &= expect_program_ast(
            "self member access binds before arithmetic",
            grammar,
            ll1_table,
            "self.x + 1;",
            "Program(\n"
            "  ExprStmt(Binary(GetAttr(Self, x), +, Number(1)))\n"
            ")");

        ok &= expect_program_ast(
            "new instantiation and postfix chain bind before arithmetic",
            grammar,
            ll1_table,
            "new Point(x, y).norm() + 1;",
            "Program(\n"
            "  ExprStmt(Binary(Call(GetAttr(New(Point(Identifier(x), Identifier(y))), norm), []), +, Number(1)))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] precedence pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
