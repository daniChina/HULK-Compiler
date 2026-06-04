#include "binding_list.hpp"

#include "../Parser/core/parse_error.hpp"

namespace semantic {

std::optional<SemanticError> findDuplicateLetBinding(const std::vector<LetBindingEntry>& bindings) {
    std::unordered_set<std::string> seen;
    for (const auto& entry : bindings) {
        const std::string& name = std::get<0>(entry).lexeme;
        if (!seen.insert(name).second) {
            const parser::Token& token = std::get<0>(entry);
            return SemanticError{
                ErrorType::REDEFINED_VARIABLE,
                "Variable '" + name + "' ya está definida en este ámbito",
                token.line,
                token.col,
                "lista de bindings de let",
                "Phase2Analyzer"};
        }
    }
    return std::nullopt;
}

void ensureUniqueLetBindingsOrThrow(const std::vector<LetBindingEntry>& bindings) {
    std::unordered_set<std::string> seen;
    for (const auto& entry : bindings) {
        const parser::Token& name_token = std::get<0>(entry);
        const std::string& name = name_token.lexeme;
        if (!seen.insert(name).second) {
            throw parser::ParseError(
                name_token,
                "Variable '" + name + "' ya está definida en este ámbito");
        }
    }
}

}  // namespace semantic
