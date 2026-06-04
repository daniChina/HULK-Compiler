#include "binding_list.hpp"

#include <vector>

namespace semantic {

namespace {
std::vector<SemanticError> pending_let_binding_errors_;
}  // namespace

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

void enqueueDuplicateLetBindingErrors(const std::vector<LetBindingEntry>& bindings) {
    if (auto err = findDuplicateLetBinding(bindings)) {
        pending_let_binding_errors_.push_back(*err);
    }
}

void drainPendingLetBindingErrors(ErrorManager& into) {
    for (const auto& err : pending_let_binding_errors_) {
        into.reportError(err.type, err.message, err.line, err.column, err.context, err.source);
    }
    pending_let_binding_errors_.clear();
}

void clearPendingLetBindingErrors() {
    pending_let_binding_errors_.clear();
}

}  // namespace semantic
