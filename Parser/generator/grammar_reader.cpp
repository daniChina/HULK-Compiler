#include "grammar_reader.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace parser::generator {
namespace {

std::string trim(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }

    const auto last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

std::vector<std::string> split_symbols(const std::string& text) {
    std::istringstream stream(text);
    std::vector<std::string> symbols;
    std::string symbol;

    while (stream >> symbol) {
        symbols.push_back(symbol);
    }

    return symbols;
}

void add_alternatives(
    Grammar& grammar,
    const std::string& lhs,
    const std::string& rhs_text,
    std::size_t line_number) {
    std::istringstream stream(rhs_text);
    std::string alternative;
    bool found_any = false;

    while (std::getline(stream, alternative, '|')) {
        found_any = true;
        alternative = trim(alternative);

        if (alternative.empty()) {
            throw std::runtime_error(
                "Alternativa vacia en linea " + std::to_string(line_number) +
                " para la produccion de " + lhs);
        }

        Production production;
        production.lhs = lhs;

        if (alternative != kEpsilonSymbol) {
            production.rhs = split_symbols(alternative);
        }

        grammar.productions.push_back(std::move(production));
    }

    if (!found_any) {
        throw std::runtime_error(
            "No se encontraron alternativas en linea " + std::to_string(line_number));
    }
}

void collect_terminals(Grammar& grammar) {
    for (const auto& production : grammar.productions) {
        for (const auto& symbol : production.rhs) {
            if (grammar.non_terminals.count(symbol) == 0) {
                grammar.terminals.insert(symbol);
            }
        }
    }
}

}  // namespace

std::string production_to_string(const Production& production) {
    std::string text = production.lhs + " ->";

    if (production.is_epsilon()) {
        text += " ";
        text += kEpsilonSymbol;
        return text;
    }

    for (const auto& symbol : production.rhs) {
        text += " ";
        text += symbol;
    }

    return text;
}

Grammar read_grammar(std::istream& input) {
    Grammar grammar;
    std::string current_lhs;
    std::string line;
    std::size_t line_number = 0;

    while (std::getline(input, line)) {
        ++line_number;
        const std::string trimmed = trim(line);

        if (trimmed.empty() || trimmed.rfind("//", 0) == 0) {
            continue;
        }

        const auto arrow = trimmed.find("->");
        if (arrow != std::string::npos) {
            const std::string lhs = trim(trimmed.substr(0, arrow));
            const std::string rhs_text = trim(trimmed.substr(arrow + 2));

            if (lhs.empty()) {
                throw std::runtime_error(
                    "Lado izquierdo vacio en linea " + std::to_string(line_number));
            }

            if (rhs_text.empty()) {
                throw std::runtime_error(
                    "Lado derecho vacio en linea " + std::to_string(line_number));
            }

            if (grammar.start_symbol.empty()) {
                grammar.start_symbol = lhs;
            }

            grammar.non_terminals.insert(lhs);
            current_lhs = lhs;
            add_alternatives(grammar, lhs, rhs_text, line_number);
            continue;
        }

        if (trimmed.front() == '|') {
            if (current_lhs.empty()) {
                throw std::runtime_error(
                    "Alternativa sin produccion base en linea " + std::to_string(line_number));
            }

            add_alternatives(grammar, current_lhs, trimmed.substr(1), line_number);
            continue;
        }

        throw std::runtime_error(
            "Formato de gramatica no reconocido en linea " + std::to_string(line_number) +
            ": " + trimmed);
    }

    if (grammar.productions.empty()) {
        throw std::runtime_error("La gramatica no contiene producciones");
    }

    collect_terminals(grammar);
    return grammar;
}

Grammar read_grammar_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("No se pudo abrir archivo de gramatica: " + path);
    }

    return read_grammar(file);
}

}  // namespace parser::generator
