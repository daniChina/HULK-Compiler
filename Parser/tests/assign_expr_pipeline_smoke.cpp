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

}  // namespace

int main() {
    try {
        const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
        const auto first_follow = parser::generator::compute_first_follow(grammar);
        const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

        bool ok = true;

        {
            const auto tokens = tokenize_source("x := 1;");
            ok &= expect_token_types(
                "lexer recognizes destructive assignment",
                tokens,
                {parser::TokenType::IDENTIFIER, parser::TokenType::ASSIGN, parser::TokenType::NUMBER_LITERAL,
                 parser::TokenType::SEMICOLON, parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline builds AssignExpr for variable assignment",
                grammar,
                ll1_table,
                "x := 1;",
                "Program(\n"
                "  ExprStmt(Assign(Identifier(x), :=, Number(1)))\n"
                ")");
        }

        ok &= expect_program_ast(
            "pipeline builds AssignExpr for member assignment",
            grammar,
            ll1_table,
            "let p = new Point(0, 0) in p.x := 1;",
            "Program(\n"
            "  ExprStmt(Let(p = New(Point(Number(0), Number(0))) in "
            "Assign(GetAttr(Identifier(p), x), :=, Number(1))))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline chains := right-associatively as AssignExpr nodes",
            grammar,
            ll1_table,
            "y := x := 5 + 5;",
            "Program(\n"
            "  ExprStmt(Assign(Identifier(y), :=, Assign(Identifier(x), :=, Binary(Number(5), +, Number(5)))))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline builds AssignExpr for self member inside type method",
            grammar,
            ll1_table,
            "type Counter[count:Number] { count:Number = 0; bump() => self.count := self.count + 1; }",
            "Program(\n"
            "  TypeDecl(Counter(count: Number) {\n"
            "    count: Number = Number(0);\n"
            "    bump() => Assign(GetAttr(Self, count), :=, Binary(GetAttr(Self, count), +, Number(1)));\n"
            "})\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] assign expressions pipeline smoke threw exception: "
                  << error.what() << "\n";
        return 1;
    }
}
