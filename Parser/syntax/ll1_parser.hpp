#pragma once

#include <string>
#include <vector>

#include "../core/parse_error.hpp"
#include "../core/token_stream.hpp"
#include "../generator/ll1_table.hpp"

namespace parser {

// Guarda una traza simple de las producciones aplicadas durante el parseo.
struct Ll1ParseResult {
    std::vector<generator::Production> derivation;
};

class Ll1Parser {
public:
    // El parser LL(1) necesita los tokens, la gramatica y la tabla ya construida.
    Ll1Parser(
        TokenList tokens,
        const generator::Grammar& grammar,
        const generator::Ll1Table& table);

    // Ejecuta el parseo predictivo completo a partir del simbolo inicial.
    Ll1ParseResult parse();

private:
    // Convierte el token actual al nombre terminal usado en grammar.ll1.
    std::string current_terminal() const;

    // Dice si un simbolo es terminal en la gramatica actual.
    bool is_terminal(const std::string& symbol) const;

    // Dice si un simbolo es no terminal en la gramatica actual.
    bool is_non_terminal(const std::string& symbol) const;

    // Consume un terminal esperado o lanza ParseError si no coincide.
    void match_terminal(const std::string& expected);

    // Expande un no terminal usando la tabla LL(1) y el lookahead actual.
    const generator::Production& select_production(const std::string& non_terminal) const;

    TokenStream tokens_;
    const generator::Grammar& grammar_;
    const generator::Ll1Table& table_;
};

}  // namespace parser
