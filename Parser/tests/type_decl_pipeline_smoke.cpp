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

// Deja visible la forma léxica exacta para depurar rápidamente alias como
// `class` -> `TYPE` o `is` en cabeceras de clases.
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

// Recorre lexer -> parser -> CST -> AST para dejar fija la forma canónica
// del árbol OO que produce el compilador.
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
            const auto tokens = tokenize_source("class Point[x:Number, y:Number] is Object[] { x = x; }");
            ok &= expect_token_types(
                "lexer recognizes class alias, bracket params and is inheritance",
                tokens,
                {parser::TokenType::TYPE, parser::TokenType::IDENTIFIER, parser::TokenType::LBRACKET,
                 parser::TokenType::IDENTIFIER, parser::TokenType::COLON, parser::TokenType::IDENTIFIER,
                 parser::TokenType::COMMA, parser::TokenType::IDENTIFIER, parser::TokenType::COLON,
                 parser::TokenType::IDENTIFIER, parser::TokenType::RBRACKET, parser::TokenType::IS,
                 parser::TokenType::IDENTIFIER, parser::TokenType::LBRACKET, parser::TokenType::RBRACKET,
                 parser::TokenType::LBRACE, parser::TokenType::IDENTIFIER, parser::TokenType::EQUAL,
                 parser::TokenType::IDENTIFIER, parser::TokenType::SEMICOLON, parser::TokenType::RBRACE,
                 parser::TokenType::EOF_TOKEN});
        }

        ok &= expect_program_ast(
            "class syntax with typed attributes reaches canonical type AST",
            grammar,
            ll1_table,
            "class Point[x:Number, y:Number] { x:Number = x; y:Number = y; norm():Number => self.x * self.x + self.y * self.y; }\n"
            "function dist(p:Point):Number => p.norm();\n"
            "new Point(3, 4).norm();",
            "Program(\n"
            "  TypeDecl(Point(x: Number, y: Number) {\n"
            "    x: Number = Identifier(x);\n"
            "    y: Number = Identifier(y);\n"
            "    norm(): Number => Binary(Binary(GetAttr(Self, x), *, GetAttr(Self, x)), +, Binary(GetAttr(Self, y), *, GetAttr(Self, y)));\n"
            "})\n"
            "  FunctionDecl(dist(p: Point): Number => Call(GetAttr(Identifier(p), norm), []))\n"
            "  ExprStmt(Call(GetAttr(New(Point(Number(3), Number(4))), norm), []))\n"
            ")");

        ok &= expect_program_ast(
            "class inheritance alias is with bracket base init and self/base bodies",
            grammar,
            ll1_table,
            "class Point(x:Number, y:Number) { x:Number = x; y:Number = y; }\n"
            "class ColoredPoint[x:Number, y:Number, c:String] is Point[x, y] {\n"
            "  color:String = c;\n"
            "  parent():Object => base(x, y);\n"
            "  label():String => self.color;\n"
            "}\n"
            "new ColoredPoint(1, 2, \"red\").label();",
            "Program(\n"
            "  TypeDecl(Point(x: Number, y: Number) {\n"
            "    x: Number = Identifier(x);\n"
            "    y: Number = Identifier(y);\n"
            "})\n"
            "  TypeDecl(ColoredPoint(x: Number, y: Number, c: String) inherits Point(Identifier(x), Identifier(y)) {\n"
            "    color: String = Identifier(c);\n"
            "    parent(): Object => BaseCall(Identifier(x), Identifier(y));\n"
            "    label(): String => GetAttr(Self, color);\n"
            "})\n"
            "  ExprStmt(Call(GetAttr(New(ColoredPoint(Number(1), Number(2), String(\"\"red\"\"))), label), []))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] type decl pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
