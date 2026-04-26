#pragma once

#include <map>
#include <string>
#include <vector>

#include "first_follow.hpp"

namespace parser::generator {

// Una tabla LL(1) es un mapa: no terminal -> terminal lookahead -> produccion.
using Ll1Table = std::map<std::string, std::map<std::string, Production>>;

// Cuando una celda ya estaba ocupada por otra produccion, se registra aqui el conflicto.
struct Ll1Conflict {
    std::string non_terminal;
    std::string terminal;
    Production existing;
    Production incoming;
};

// Resultado completo de construir la tabla a partir de Grammar + FIRST/FOLLOW.
struct Ll1TableResult {
    Ll1Table table;
    std::vector<Ll1Conflict> conflicts;
};

Ll1TableResult build_ll1_table(
    const Grammar& grammar,
    const FirstFollowResult& first_follow);

std::string conflict_to_string(const Ll1Conflict& conflict);

}  // namespace parser::generator
