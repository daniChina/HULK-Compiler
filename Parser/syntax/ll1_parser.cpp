#include "ll1_parser.hpp"

#include <stdexcept>
#include <utility>

namespace parser {
namespace {

// Item de pila: el simbolo a procesar y el nodo del CST asociado a ese simbolo.
struct StackItem {
    std::string symbol;
    CstNode* node = nullptr;
};

}  // namespace

Ll1Parser::Ll1Parser(
    TokenList tokens,
    const generator::Grammar& grammar,
    const generator::Ll1Table& table)
    : tokens_(std::move(tokens)),
      grammar_(grammar),
      table_(table) {}

Ll1ParseResult Ll1Parser::parse() {
    Ll1ParseResult result;
    result.cst_root = std::make_unique<CstNode>(grammar_.start_symbol);

    // La pila arranca con el simbolo inicial y el nodo raiz del CST.
    std::vector<StackItem> stack;
    stack.push_back(StackItem{grammar_.start_symbol, result.cst_root.get()});

    while (!stack.empty()) {
        const StackItem current = stack.back();
        stack.pop_back();
        const std::string& top = current.symbol;

        if (is_terminal(top)) {
            current.node->has_token = true;
            current.node->token = tokens_.current();
            match_terminal(top);
            continue;
        }

        if (!is_non_terminal(top)) {
            throw std::runtime_error("Simbolo desconocido en la pila LL(1): " + top);
        }

        const auto& production = select_production(top);
        result.derivation.push_back(production);

        // El epsilon se materializa como un hijo explicito para reflejar la derivacion.
        if (production.is_epsilon()) {
            current.node->add_child(std::make_unique<CstNode>(generator::kEpsilonSymbol));
            continue;
        }

        std::vector<StackItem> children_to_push;

        // Los hijos se crean en orden natural para preservar el arbol.
        for (const auto& symbol : production.rhs) {
            CstNode* child =
                current.node->add_child(std::make_unique<CstNode>(symbol));
            children_to_push.push_back(StackItem{symbol, child});
        }

        // El lado derecho se empuja al reves para que el primer simbolo quede arriba.
        for (auto it = children_to_push.rbegin(); it != children_to_push.rend(); ++it) {
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
