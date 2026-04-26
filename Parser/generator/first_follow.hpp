#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "production.hpp"

namespace parser::generator {

using SymbolSet = std::set<std::string>;
using SymbolSetMap = std::map<std::string, SymbolSet>;

struct FirstFollowResult {
    SymbolSetMap first_sets;
    SymbolSetMap follow_sets;
};

FirstFollowResult compute_first_follow(const Grammar& grammar);
SymbolSet compute_first_of_sequence(
    const std::vector<std::string>& sequence,
    const SymbolSetMap& first_sets,
    const Grammar& grammar);

}  // namespace parser::generator
