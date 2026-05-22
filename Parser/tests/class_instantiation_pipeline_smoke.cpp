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

// Este helper deja la forma de tokens visible para ubicar rápido si el fallo
// de instanciación viene del lexer o del parser.
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

// Valida el pipeline completo lexer -> parser -> CST -> AST para las formas
// de instanciación soportadas por la gramática actual.
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

        std::cout << "[OK] " << name << "\n";
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
            const auto tokens = tokenize_source("let p = new Point() in print(p.translate(5,3).length());");
            ok &= expect_token_types(
                "lexer recognizes no-arg class instantiation and chained calls",
                tokens,
                {parser::TokenType::LET, parser::TokenType::IDENTIFIER, parser::TokenType::EQUAL,
                 parser::TokenType::NEW, parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN,
                 parser::TokenType::RPAREN, parser::TokenType::IN, parser::TokenType::IDENTIFIER,
                 parser::TokenType::LPAREN, parser::TokenType::IDENTIFIER, parser::TokenType::DOT,
                 parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN, parser::TokenType::NUMBER_LITERAL,
                 parser::TokenType::COMMA, parser::TokenType::NUMBER_LITERAL, parser::TokenType::RPAREN,
                 parser::TokenType::DOT, parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN,
                 parser::TokenType::RPAREN, parser::TokenType::RPAREN, parser::TokenType::SEMICOLON,
                 parser::TokenType::EOF_TOKEN});
        }

        ok &= expect_program_ast(
            "pipeline parses no-arg instantiation and chained method calls",
            grammar,
            ll1_table,
            "class Point {\n"
            "  x:Number = 0;\n"
            "  y:Number = 0;\n"
            "  translate(dx:Number,dy:Number):Point => new Point(self.x + dx, self.y + dy);\n"
            "  length():Number => sqrt(self.x * self.x + self.y * self.y);\n"
            "}\n"
            "let p = new Point() in print(p.translate(5,3).length());",
            "Program(\n"
            "  TypeDecl(Point() {\n"
            "    x: Number = Number(0);\n"
            "    y: Number = Number(0);\n"
            "    translate(dx: Number, dy: Number): Point => New(Point(Binary(GetAttr(Self, x), +, Identifier(dx)), Binary(GetAttr(Self, y), +, Identifier(dy))));\n"
            "    length(): Number => Call(Identifier(sqrt), [Binary(Binary(GetAttr(Self, x), *, GetAttr(Self, x)), +, Binary(GetAttr(Self, y), *, GetAttr(Self, y)))]);\n"
            "})\n"
            "  ExprStmt(Let(p = New(Point()) in Call(Identifier(print), [Call(GetAttr(Call(GetAttr(Identifier(p), translate), [Number(5), Number(3)]), length), [])])))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline parses instantiation with class arguments used in attribute init",
            grammar,
            ll1_table,
            "class Point(x:Number, y:Number) {\n"
            "  x:Number = x;\n"
            "  y:Number = y;\n"
            "  length():Number => sqrt(self.x * self.x + self.y * self.y);\n"
            "}\n"
            "let p:Point = new Point(5,3) in print(p.length());",
            "Program(\n"
            "  TypeDecl(Point(x: Number, y: Number) {\n"
            "    x: Number = Identifier(x);\n"
            "    y: Number = Identifier(y);\n"
            "    length(): Number => Call(Identifier(sqrt), [Binary(Binary(GetAttr(Self, x), *, GetAttr(Self, x)), +, Binary(GetAttr(Self, y), *, GetAttr(Self, y)))]);\n"
            "})\n"
            "  ExprStmt(Let(p: Point = New(Point(Number(5), Number(3))) in Call(Identifier(print), [Call(GetAttr(Identifier(p), length), [])])))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] class instantiation pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
