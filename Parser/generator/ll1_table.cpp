#include "ll1_table.hpp"

#include <stdexcept>

namespace parser::generator {
namespace {

// Inserta una produccion en una celda de la tabla y detecta si esa celda ya tenia otra.
void insert_table_entry(
    Ll1TableResult& result,
    const std::string& non_terminal,
    const std::string& terminal,
    const Production& production) {
    auto& row = result.table[non_terminal];
    const auto found = row.find(terminal);

    if (found == row.end()) {
        row.emplace(terminal, production);
        return;
    }

    if (found->second.lhs == production.lhs && found->second.rhs == production.rhs) {
        return;
    }

    result.conflicts.push_back(Ll1Conflict{
        non_terminal,
        terminal,
        found->second,
        production,
    });
}

}  // namespace

std::string conflict_to_string(const Ll1Conflict& conflict) {
    return "Conflicto LL(1) en M[" + conflict.non_terminal + ", " + conflict.terminal + "] entre {" +
           production_to_string(conflict.existing) + "} y {" +
           production_to_string(conflict.incoming) + "}";
}

Ll1TableResult build_ll1_table(
    const Grammar& grammar,
    const FirstFollowResult& first_follow) {
    Ll1TableResult result;

    for (const auto& production : grammar.productions) {
        const auto rhs_first =
            compute_first_of_sequence(production.rhs, first_follow.first_sets, grammar);

        // Regla 1: por cada terminal en FIRST(alpha) distinto de epsilon,
        // se agrega A -> alpha en M[A, terminal].
        for (const auto& symbol : rhs_first) {
            if (symbol == kEpsilonSymbol) {
                continue;
            }

            insert_table_entry(result, production.lhs, symbol, production);
        }

        // Regla 2: si alpha puede derivar epsilon, se usan los terminales de FOLLOW(A).
        if (rhs_first.count(kEpsilonSymbol) == 1) {
            const auto follow_found = first_follow.follow_sets.find(production.lhs);
            if (follow_found == first_follow.follow_sets.end()) {
                throw std::runtime_error(
                    "No existe FOLLOW para el no terminal: " + production.lhs);
            }

            for (const auto& follow_symbol : follow_found->second) {
                insert_table_entry(result, production.lhs, follow_symbol, production);
            }
        }
    }

    return result;
}

}  // namespace parser::generator
