#include "ll1_parser.hpp"

#include <stdexcept>

namespace parser {

Ll1Parser::Ll1Parser(
    TokenList tokens,
    const generator::Grammar& grammar,
    const generator::Ll1Table& table)
    : tokens_(std::move(tokens)),
      grammar_(grammar),
      table_(table) {}

Ll1ParseResult Ll1Parser::parse() {
    // La pila arranca con el simbolo inicial y se procesa en orden LIFO.
    std::vector<std::string> stack;
    stack.push_back(grammar_.start_symbol);

    Ll1ParseResult result;

    while (!stack.empty()) {
        const std::string top = stack.back();
        stack.pop_back();

        if (is_terminal(top)) {
            match_terminal(top);
            continue;
        }

        if (!is_non_terminal(top)) {
            throw std::runtime_error("Simbolo desconocido en la pila LL(1): " + top);
        }

        const auto& production = select_production(top);
        result.derivation.push_back(production);

        // El lado derecho se empuja al reves para que el primer simbolo quede arriba.
        for (auto it = production.rhs.rbegin(); it != production.rhs.rend(); ++it) {
            stack.push_back(*it);
        }
    }

    // Si todavia quedan tokens distintos de EOF, hubo entrada sin consumir.
    if (!tokens_.at_end()) {
        throw ParseError(tokens_.current(), "Quedaron tokens sin consumir al final del parseo LL(1)");
    }

    return result;
}

std::string Ll1Parser::current_terminal() const {
    return token_name(tokens_.current().type);
}

bool Ll1Parser::is_terminal(const std::string& symbol) const {
    return grammar_.terminals.count(symbol) == 1;
}

bool Ll1Parser::is_non_terminal(const std::string& symbol) const {
    return grammar_.non_terminals.count(symbol) == 1;
}

void Ll1Parser::match_terminal(const std::string& expected) {
    const auto actual = current_terminal();
    if (actual != expected) {
        throw ParseError(
            tokens_.current(),
            "Se esperaba el terminal " + expected + " pero se encontro " + actual);
    }

    // EOF se considera consumido logicamente al finalizar; no hace falta avanzar.
    if (expected == "EOF_TOKEN") {
        return;
    }

    tokens_.advance();
}

const generator::Production& Ll1Parser::select_production(const std::string& non_terminal) const {
    const auto row = table_.find(non_terminal);
    if (row == table_.end()) {
        throw ParseError(
            tokens_.current(),
            "No existe fila LL(1) para el no terminal " + non_terminal);
    }

    const auto lookahead = current_terminal();
    const auto entry = row->second.find(lookahead);
    if (entry == row->second.end()) {
        throw ParseError(
            tokens_.current(),
            "No hay produccion LL(1) para " + non_terminal + " con lookahead " + lookahead);
    }

    return entry->second;
}

}  // namespace parser
