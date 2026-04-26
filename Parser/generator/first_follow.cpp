#include "first_follow.hpp"

#include <stdexcept>

namespace parser::generator {
namespace {

bool is_non_terminal(const Grammar& grammar, const std::string& symbol) {
    return grammar.non_terminals.count(symbol) == 1;
}

bool is_terminal(const Grammar& grammar, const std::string& symbol) {
    return grammar.terminals.count(symbol) == 1;
}

bool insert_all_except_epsilon(SymbolSet& destination, const SymbolSet& source) {
    bool changed = false;

    for (const auto& symbol : source) {
        if (symbol == kEpsilonSymbol) {
            continue;
        }

        changed = destination.insert(symbol).second || changed;
    }

    return changed;
}

SymbolSetMap initialize_first_sets(const Grammar& grammar) {
    SymbolSetMap first_sets;

    for (const auto& terminal : grammar.terminals) {
        first_sets[terminal].insert(terminal);
    }

    for (const auto& non_terminal : grammar.non_terminals) {
        first_sets[non_terminal];
    }

    return first_sets;
}

SymbolSetMap initialize_follow_sets(const Grammar& grammar) {
    SymbolSetMap follow_sets;

    for (const auto& non_terminal : grammar.non_terminals) {
        follow_sets[non_terminal];
    }

    follow_sets[grammar.start_symbol].insert("EOF_TOKEN");
    return follow_sets;
}

}  // namespace

SymbolSet compute_first_of_sequence(
    const std::vector<std::string>& sequence,
    const SymbolSetMap& first_sets,
    const Grammar& grammar) {
    SymbolSet result;

    if (sequence.empty()) {
        result.insert(kEpsilonSymbol);
        return result;
    }

    bool all_can_derive_epsilon = true;

    for (const auto& symbol : sequence) {
        if (symbol == kEpsilonSymbol) {
            result.insert(kEpsilonSymbol);
            return result;
        }

        if (is_terminal(grammar, symbol)) {
            result.insert(symbol);
            all_can_derive_epsilon = false;
            break;
        }

        const auto found = first_sets.find(symbol);
        if (found == first_sets.end()) {
            throw std::runtime_error("No existe FIRST para el simbolo: " + symbol);
        }

        const auto& symbol_first = found->second;
        insert_all_except_epsilon(result, symbol_first);

        if (symbol_first.count(kEpsilonSymbol) == 0) {
            all_can_derive_epsilon = false;
            break;
        }
    }

    if (all_can_derive_epsilon) {
        result.insert(kEpsilonSymbol);
    }

    return result;
}

FirstFollowResult compute_first_follow(const Grammar& grammar) {
    if (grammar.start_symbol.empty()) {
        throw std::runtime_error("La gramatica no tiene simbolo inicial");
    }

    FirstFollowResult result;
    result.first_sets = initialize_first_sets(grammar);
    result.follow_sets = initialize_follow_sets(grammar);

    bool changed = true;
    while (changed) {
        changed = false;

        for (const auto& production : grammar.productions) {
            auto& lhs_first = result.first_sets[production.lhs];

            if (production.is_epsilon()) {
                changed = lhs_first.insert(kEpsilonSymbol).second || changed;
                continue;
            }

            const auto rhs_first = compute_first_of_sequence(production.rhs, result.first_sets, grammar);
            for (const auto& symbol : rhs_first) {
                changed = lhs_first.insert(symbol).second || changed;
            }
        }
    }

    changed = true;
    while (changed) {
        changed = false;

        for (const auto& production : grammar.productions) {
            for (std::size_t i = 0; i < production.rhs.size(); ++i) {
                const auto& symbol = production.rhs[i];
                if (!is_non_terminal(grammar, symbol)) {
                    continue;
                }

                std::vector<std::string> suffix(
                    production.rhs.begin() + static_cast<std::ptrdiff_t>(i + 1),
                    production.rhs.end());

                auto& follow_symbol = result.follow_sets[symbol];
                const auto suffix_first =
                    compute_first_of_sequence(suffix, result.first_sets, grammar);

                changed = insert_all_except_epsilon(follow_symbol, suffix_first) || changed;

                if (suffix.empty() || suffix_first.count(kEpsilonSymbol) == 1) {
                    const auto& lhs_follow = result.follow_sets[production.lhs];
                    for (const auto& follow_item : lhs_follow) {
                        changed = follow_symbol.insert(follow_item).second || changed;
                    }
                }
            }
        }
    }

    return result;
}

}  // namespace parser::generator
