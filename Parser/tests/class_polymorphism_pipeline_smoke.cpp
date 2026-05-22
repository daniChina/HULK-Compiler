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

// Muestra los tipos de token para dejar claro que la forma de herencia
// y redefinición llega bien desde el lexer.
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

// Recorre lexer -> parser -> CST -> AST para fijar qué parte del
// polimorfismo/redefinición se soporta ya en sintaxis.
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
            const auto tokens = tokenize_source("class Colleague is Person { greet() -> \"Hi\" @@ self.name; }");
            ok &= expect_token_types(
                "lexer recognizes inherited class and overriding method syntax",
                tokens,
                {parser::TokenType::TYPE, parser::TokenType::IDENTIFIER, parser::TokenType::IS,
                 parser::TokenType::IDENTIFIER, parser::TokenType::LBRACE, parser::TokenType::IDENTIFIER,
                 parser::TokenType::LPAREN, parser::TokenType::RPAREN, parser::TokenType::ARROW,
                 parser::TokenType::STRING_LITERAL, parser::TokenType::CONCAT_WS, parser::TokenType::SELF,
                 parser::TokenType::DOT, parser::TokenType::IDENTIFIER, parser::TokenType::SEMICOLON,
                 parser::TokenType::RBRACE, parser::TokenType::EOF_TOKEN});
        }

        ok &= expect_program_ast(
            "pipeline parses implicit method override and inherited constructor args at call site",
            grammar,
            ll1_table,
            "class Person(name:String) {\n"
            "  name:String = name;\n"
            "  greet():String -> \"Hello\" @@ self.name;\n"
            "}\n"
            "class Colleague is Person {\n"
            "  greet():String -> \"Hi\" @@ self.name;\n"
            "}\n"
            "let p:Person = new Colleague(\"Pete\") in print(p.greet());",
            "Program(\n"
            "  TypeDecl(Person(name: String) {\n"
            "    name: String = Identifier(name);\n"
            "    greet(): String => Binary(String(\"\"Hello\"\"), @@, GetAttr(Self, name));\n"
            "})\n"
            "  TypeDecl(Colleague() inherits Person() {\n"
            "    greet(): String => Binary(String(\"\"Hi\"\"), @@, GetAttr(Self, name));\n"
            "})\n"
            "  ExprStmt(Let(p: Person = New(Colleague(String(\"\"Pete\"\"))) in Call(Identifier(print), [Call(GetAttr(Identifier(p), greet), [])])))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline parses explicit base argument computation in inherited class header",
            grammar,
            ll1_table,
            "class Person(name:String) {\n"
            "  name:String = name;\n"
            "  greet():String -> \"Hello\" @@ self.name;\n"
            "}\n"
            "class Noble(title:String, who:String) is Person(title @@ who) { }\n"
            "let p = new Noble(\"Sir\", \"Thomas\") in print(p.greet());",
            "Program(\n"
            "  TypeDecl(Person(name: String) {\n"
            "    name: String = Identifier(name);\n"
            "    greet(): String => Binary(String(\"\"Hello\"\"), @@, GetAttr(Self, name));\n"
            "})\n"
            "  TypeDecl(Noble(title: String, who: String) inherits Person(Binary(Identifier(title), @@, Identifier(who))) {\n"
            "})\n"
            "  ExprStmt(Let(p = New(Noble(String(\"\"Sir\"\"), String(\"\"Thomas\"\"))) in Call(Identifier(print), [Call(GetAttr(Identifier(p), greet), [])])))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] class polymorphism pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
