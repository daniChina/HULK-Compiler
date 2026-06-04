#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "../Parser/ast/expr.hpp"
#include "../Parser/core/token.hpp"
#include "error.hpp"

namespace semantic {

using LetBindingEntry = std::tuple<parser::Token, std::optional<parser::Token>, parser::ExprPtr>;

// R1: misma lista `let a, b, c` — un nombre no puede repetirse antes del desazucarado.
// Devuelve el error con la posición del binding duplicado (segunda aparición).
std::optional<SemanticError> findDuplicateLetBinding(const std::vector<LetBindingEntry>& bindings);

// Registra duplicados para reportarlos en --semantic (paridad amabe; ver fase2_decisiones_paridad.md).
void enqueueDuplicateLetBindingErrors(const std::vector<LetBindingEntry>& bindings);

void drainPendingLetBindingErrors(ErrorManager& into);
void clearPendingLetBindingErrors();

}  // namespace semantic
