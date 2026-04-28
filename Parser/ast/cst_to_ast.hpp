#pragma once

#include "cst_nodes.hpp"
#include "expr.hpp"

namespace parser {

// Convierte la raiz `Program` del CST actual en el AST de la expresion principal.
ExprPtr cst_to_ast(const CstNode& root);

}  // namespace parser
