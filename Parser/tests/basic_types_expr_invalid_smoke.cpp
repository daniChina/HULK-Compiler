#include <exception>
#include <iostream>
#include <sstream>
#include <string>

#include "../ast/cst_to_ast.hpp"
#include "../core/parse_error.hpp"
#include "../core/token_adapter.hpp"
#include "../generator/first_follow.hpp"
#include "../generator/grammar_reader.hpp"
#include "../generator/ll1_table.hpp"
#include "../syntax/ll1_parser.hpp"

namespace {

// Helper simple para dejar claro cuando un caso invalido fue rechazado
// correctamente por el pipeline completo.
bool expect_parse_error(
    const std::string& name,
    const parser::generator::Grammar& grammar,
    const parser::generator::Ll1TableResult& ll1_table,
    const std::string& source) {
    try {
        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto result = parser.parse();
        (void)result;

        std::cerr << "[FAIL] " << name << "\n"
                  << "  se esperaba ParseError y el programa fue aceptado\n";
        return false;
    } catch (const parser::ParseError& error) {
        std::cout << "[OK] " << name << " -> " << error.what() << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  excepcion inesperada: " << error.what() << "\n";
        return false;
    }
}

// Este helper se usa para documentar el alcance de la fase actual:
// una expresion puede ser semantica o tipadamente invalida, pero si su forma
// sintactica es correcta, el parser debe aceptarla en esta etapa.
bool expect_parse_ok(
    const std::string& name,
    const parser::generator::Grammar& grammar,
    const parser::generator::Ll1TableResult& ll1_table,
    const std::string& source) {
    try {
        std::istringstream input(source);
        auto tokens = parser::tokenize_stream(input);
        parser::Ll1Parser parser(std::move(tokens), grammar, ll1_table.table);
        const auto result = parser.parse();
        auto program = parser::cst_to_ast(*result.cst_root);
        std::cout << "[OK] " << name << " -> " << parser::program_to_string(*program) << "\n";
        return true;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << name << "\n"
                  << "  se esperaba aceptacion sintactica y ocurrio: " << error.what() << "\n";
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

        // Falta operando derecho despues de un operador aritmetico basico.
        ok &= expect_parse_error(
            "rejects missing rhs after plus",
            grammar,
            ll1_table,
            "42 + ;");

        // El nuevo operador modulo debe fallar igual que * o / si queda colgando.
        ok &= expect_parse_error(
            "rejects missing rhs after modulo",
            grammar,
            ll1_table,
            "10 % ;");

        // No se puede empezar una expresion con un operador binario.
        ok &= expect_parse_error(
            "rejects expression starting with binary operator",
            grammar,
            ll1_table,
            "* 3;");

        // Una negacion unaria sin operando tambien debe producir error.
        ok &= expect_parse_error(
            "rejects unary operator without operand",
            grammar,
            ll1_table,
            "! ;");

        // Los parentesis agrupados deben cerrarse.
        ok &= expect_parse_error(
            "rejects unclosed grouped expression",
            grammar,
            ll1_table,
            "(3 + 4;");

        // Dos operandos seguidos sin operador entre ellos no forman una expresion.
        ok &= expect_parse_error(
            "rejects adjacent literals without operator",
            grammar,
            ll1_table,
            "42 43;");

        // Un operador de concatenacion sin operando derecho debe ser rechazado.
        ok &= expect_parse_error(
            "rejects missing rhs after concat_ws",
            grammar,
            ll1_table,
            "\"hola\" @@ ;");

        // Las comparaciones tambien requieren operando derecho.
        ok &= expect_parse_error(
            "rejects missing rhs after comparison",
            grammar,
            ll1_table,
            "1 <= ;");

        // Una cadena de igualdad incompleta debe fallar.
        ok &= expect_parse_error(
            "rejects missing rhs after equality",
            grammar,
            ll1_table,
            "Null != ;");

        // El parser actual solo valida sintaxis. Esta mezcla es discutible
        // semantica o tipadamente, pero su forma sintactica es correcta y por
        // eso debe aceptarse en esta fase.
        ok &= expect_parse_ok(
            "accepts syntactically valid mixed-type expression",
            grammar,
            ll1_table,
            "\"hola\" + 1;");

        return ok ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] basic invalid expressions smoke threw exception: "
                  << error.what() << "\n";
        return 1;
    }
}
