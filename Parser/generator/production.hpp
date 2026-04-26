#pragma once

#include <set>
#include <string>
#include <vector>

namespace parser::generator {

inline constexpr const char* kEpsilonSymbol = "ε";

struct Production {
    std::string lhs;
    std::vector<std::string> rhs;

    bool is_epsilon() const { return rhs.empty(); }
};

struct Grammar {
    std::string start_symbol;
    std::vector<Production> productions;
    std::set<std::string> non_terminals;
    std::set<std::string> terminals;
};

std::string production_to_string(const Production& production);

}  // namespace parser::generator
