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

// Muestra los tokens relevantes para diagnosticar rápido el keyword `case`
// y la palabra `of` antes de revisar gramática o AST.
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

// Recorre lexer -> parser -> CST -> AST para fijar el soporte sintáctico
// de `case` en su forma compacta y en su forma con bloque de ramas.
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
            const auto tokens = tokenize_source(
                "case new C() of { a:A -> print(\"A\"); b:B -> print(\"B\"); d:D -> print(\"D\"); };");
            ok &= expect_token_types(
                "lexer recognizes case block syntax",
                tokens,
                {parser::TokenType::CASE, parser::TokenType::NEW, parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN,
                 parser::TokenType::RPAREN, parser::TokenType::OF, parser::TokenType::LBRACE, parser::TokenType::IDENTIFIER,
                 parser::TokenType::COLON, parser::TokenType::IDENTIFIER, parser::TokenType::ARROW, parser::TokenType::IDENTIFIER,
                 parser::TokenType::LPAREN, parser::TokenType::STRING_LITERAL, parser::TokenType::RPAREN, parser::TokenType::SEMICOLON,
                 parser::TokenType::IDENTIFIER, parser::TokenType::COLON, parser::TokenType::IDENTIFIER, parser::TokenType::ARROW,
                 parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN, parser::TokenType::STRING_LITERAL, parser::TokenType::RPAREN,
                 parser::TokenType::SEMICOLON, parser::TokenType::IDENTIFIER, parser::TokenType::COLON, parser::TokenType::IDENTIFIER,
                 parser::TokenType::ARROW, parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN, parser::TokenType::STRING_LITERAL,
                 parser::TokenType::RPAREN, parser::TokenType::SEMICOLON, parser::TokenType::RBRACE, parser::TokenType::SEMICOLON,
                 parser::TokenType::EOF_TOKEN});
        }

        ok &= expect_program_ast(
            "pipeline parses block case with multiple typed branches",
            grammar,
            ll1_table,
            "class A { }\n"
            "class B is A { }\n"
            "class C is B { }\n"
            "class D is A { }\n"
            "case new C() of {\n"
            "  a:A -> print(\"A\");\n"
            "  b:B -> print(\"B\");\n"
            "  d:D -> print(\"D\");\n"
            "};",
            "Program(\n"
            "  TypeDecl(A() {\n"
            "})\n"
            "  TypeDecl(B() inherits A() {\n"
            "})\n"
            "  TypeDecl(C() inherits B() {\n"
            "})\n"
            "  TypeDecl(D() inherits A() {\n"
            "})\n"
            "  ExprStmt(Case(New(C()), [a: A => Call(Identifier(print), [String(\"\"A\"\")]), b: B => Call(Identifier(print), [String(\"\"B\"\")]), d: D => Call(Identifier(print), [String(\"\"D\"\")])]))\n"
            ")");

        ok &= expect_program_ast(
            "pipeline parses compact case useful for downcast syntax",
            grammar,
            ll1_table,
            "function dunno():Object -> 40;\n"
            "let o:Object = something() in case o of y:Number -> y + 2;",
            "Program(\n"
            "  FunctionDecl(dunno(): Object => Number(40))\n"
            "  ExprStmt(Let(o: Object = Call(Identifier(something), []) in Case(Identifier(o), [y: Number => Binary(Identifier(y), +, Number(2))])))\n"
            ")");

        ok &= expect_program_ast(
            "case branch body can be a block expression",
            grammar,
            ll1_table,
            "case x of { y:Number -> { print(y); y + 2; }; };",
            "Program(\n"
            "  ExprStmt(Case(Identifier(x), [y: Number => Block(Call(Identifier(print), [Identifier(y)]), Binary(Identifier(y), +, Number(2)))]))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] case expr pipeline smoke threw exception: " << error.what() << "\n";
        return 1;
    }
}
