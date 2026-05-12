#include <exception>
#include <initializer_list>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "../ast/cst_to_ast.hpp"
#include "../core/token.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

using parser::TokenList;

// Valida una condicion y deja una traza simple para poder identificar rapido
// que parte del pipeline fallo.
bool expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }

    std::cout << "[OK] " << message << "\n";
    return true;
}

// Convierte una lista de tipos a un formato simple para compararla en mensajes.
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

// Ejecuta el lexer real sobre una cadena para que la prueba cubra la entrada
// textual del lenguaje y no una secuencia de tokens armada a mano.
TokenList tokenize_source(const std::string& source) {
    std::istringstream input(source);
    return parser::tokenize_stream(input);
}

// Comprueba solo la forma general de los tokens relevantes para el caso.
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

// Recorre todo el pipeline del parser generado: lexer -> tabla LL(1) -> CST -> AST.
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

        // Caso 1: valida que un entero simple llegue como NUMBER_LITERAL y termine
        // convertido en un Program con una sola ExprStmt numerica.
        {
            const auto tokens = tokenize_source("42;");
            ok &= expect_token_types(
                "lexer recognizes integer literal",
                tokens,
                {parser::TokenType::NUMBER_LITERAL, parser::TokenType::SEMICOLON, parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline builds AST for integer literal",
                grammar,
                ll1_table,
                "42;",
                "Program(\n"
                "  ExprStmt(Number(42))\n"
                ")");
        }

        // Caso 2: comprueba el mismo flujo para un numero flotante, que en HULK
        // se representa con el mismo token sintactico NUMBER_LITERAL.
        {
            const auto tokens = tokenize_source("3.14;");
            ok &= expect_token_types(
                "lexer recognizes floating literal",
                tokens,
                {parser::TokenType::NUMBER_LITERAL, parser::TokenType::SEMICOLON, parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline builds AST for floating literal",
                grammar,
                ll1_table,
                "3.14;",
                "Program(\n"
                "  ExprStmt(Number(3.14))\n"
                ")");
        }

        // Caso 3: cubre los tres literales basicos ya soportados por el parser:
        // String, true y false, todos dentro del mismo programa.
        ok &= expect_program_ast(
            "pipeline accepts basic literal statements",
            grammar,
            ll1_table,
            "\"hola\"; true; false;",
            "Program(\n"
            "  ExprStmt(String(\"\"hola\"\"))\n"
            "  ExprStmt(Bool(true))\n"
            "  ExprStmt(Bool(false))\n"
            ")");

        // Caso 3b: valida el nuevo literal Null en el pipeline completo y
        // comprueba que el lexer lo entregue como token propio.
        {
            const auto tokens = tokenize_source("Null; null;");
            ok &= expect_token_types(
                "lexer recognizes Null literal in both accepted spellings",
                tokens,
                {parser::TokenType::NULL_LITERAL, parser::TokenType::SEMICOLON,
                 parser::TokenType::NULL_LITERAL, parser::TokenType::SEMICOLON,
                 parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline builds AST for null literal statements",
                grammar,
                ll1_table,
                "Null; null;",
                "Program(\n"
                "  ExprStmt(Null)\n"
                "  ExprStmt(Null)\n"
                ")");
        }

        // Caso 4: valida precedencia aritmetica y agrupacion con parentesis.
        ok &= expect_program_ast(
            "pipeline preserves arithmetic precedence and grouping",
            grammar,
            ll1_table,
            "(2 + 3) * 4 - 5 / 6;",
            "Program(\n"
            "  ExprStmt(Binary(Binary(Grouped(Binary(Number(2), +, Number(3))), *, Number(4)), -, Binary(Number(5), /, Number(6))))\n"
            ")");

        // Caso 4b: `%` debe compartir el nivel multiplicativo con `*` y `/`.
        {
            const auto tokens = tokenize_source("10 % 3 * 2;");
            ok &= expect_token_types(
                "lexer recognizes modulo operator",
                tokens,
                {parser::TokenType::NUMBER_LITERAL, parser::TokenType::PERCENT, parser::TokenType::NUMBER_LITERAL,
                 parser::TokenType::STAR, parser::TokenType::NUMBER_LITERAL, parser::TokenType::SEMICOLON,
                 parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline parses modulo with multiplicative precedence",
                grammar,
                ll1_table,
                "10 % 3 * 2;",
                "Program(\n"
                "  ExprStmt(Binary(Binary(Number(10), %, Number(3)), *, Number(2)))\n"
                ")");
        }

        // Caso 5: comprueba la parte logica implementada en el parser:
        // negacion, AND y OR con su precedencia relativa actual.
        ok &= expect_program_ast(
            "pipeline preserves logical precedence",
            grammar,
            ll1_table,
            "!true and false or x;",
            "Program(\n"
            "  ExprStmt(Binary(Binary(Unary(!, Bool(true)), and, Bool(false)), or, Identifier(x)))\n"
            ")");

        // Caso 6: verifica que las comparaciones queden por encima del nivel
        // aritmetico y que los tokens compuestos del lexer entren bien al parser.
        ok &= expect_program_ast(
            "pipeline parses arithmetic inside comparisons",
            grammar,
            ll1_table,
            "1 + 2 <= 3 * 4;",
            "Program(\n"
            "  ExprStmt(Binary(Binary(Number(1), +, Number(2)), <=, Binary(Number(3), *, Number(4))))\n"
            ")");

        // Caso 7: cubre igualdad y desigualdad, que en la gramatica actual usan
        // el mismo nivel sintactico de comparacion.
        ok &= expect_program_ast(
            "pipeline parses equality operators",
            grammar,
            ll1_table,
            "\"hola\" == \"hola\" != false;",
            "Program(\n"
            "  ExprStmt(Binary(Binary(String(\"\"hola\"\"), ==, String(\"\"hola\"\")), !=, Bool(false)))\n"
            ")");

        // Caso 8: valida los dos operadores de concatenacion soportados por el
        // lexer y por la gramatica LL(1): @ y @@.
        ok &= expect_program_ast(
            "pipeline parses string concatenation operators",
            grammar,
            ll1_table,
            "\"Hello\" @ \"World\" @@ \"!\";",
            "Program(\n"
            "  ExprStmt(Binary(Binary(String(\"\"Hello\"\"), @, String(\"\"World\"\")), @@, String(\"\"!\"\")))\n"
            ")");

        // Caso 9: comprueba que nombres globales como print y parse siguen
        // llegando como IDENTIFIER y pueden usarse en llamadas postfix.
        {
            const auto tokens = tokenize_source("print(parse(42));");
            ok &= expect_token_types(
                "lexer keeps global builtins as identifiers",
                tokens,
                {parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN, parser::TokenType::IDENTIFIER, parser::TokenType::LPAREN,
                 parser::TokenType::NUMBER_LITERAL, parser::TokenType::RPAREN, parser::TokenType::RPAREN,
                 parser::TokenType::SEMICOLON, parser::TokenType::EOF_TOKEN});

        ok &= expect_program_ast(
            "pipeline parses calls to global builtins",
            grammar,
            ll1_table,
            "print(parse(42));",
            "Program(\n"
                "  ExprStmt(Call(Identifier(print), [Call(Identifier(parse), [Number(42)])]))\n"
                ")");
        }

        // Caso 9b: valida que tambien se acepten identificadores cuyo primer
        // caracter es `_`, como pide la especificacion.
        {
            const auto tokens = tokenize_source("_ugly_case;");
            ok &= expect_token_types(
                "lexer recognizes identifier starting with underscore",
                tokens,
                {parser::TokenType::IDENTIFIER, parser::TokenType::SEMICOLON, parser::TokenType::EOF_TOKEN});

            ok &= expect_program_ast(
                "pipeline parses identifier starting with underscore",
                grammar,
                ll1_table,
                "_ugly_case;",
                "Program(\n"
                "  ExprStmt(Identifier(_ugly_case))\n"
                ")");
        }

        // Caso 10: mezcla grande para comprobar que todos los niveles de
        // precedencia convivan bien en una sola expresion.
        ok &= expect_program_ast(
            "pipeline parses mixed operators across all implemented levels",
            grammar,
            ll1_table,
            "!(1 + 2 * 3 % 4 < 5) or \"a\" @@ \"b\" == \"a b\" and Null != x;",
            "Program(\n"
            "  ExprStmt(Binary(Unary(!, Grouped(Binary(Binary(Number(1), +, Binary(Binary(Number(2), *, Number(3)), %, Number(4))), <, Number(5)))), or, Binary(Binary(Binary(String(\"\"a\"\"), @@, String(\"\"b\"\")), ==, String(\"\"a b\"\")), and, Binary(Null, !=, Identifier(x)))))\n"
            ")");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] basic types / expressions pipeline smoke threw exception: "
                  << error.what() << "\n";
        return 1;
    }
}
