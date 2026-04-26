#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../core/token.hpp"

namespace parser {

// Nodo base del CST. Guarda el simbolo gramatical y, si aplica, el token consumido.
struct CstNode {
    explicit CstNode(std::string symbol);

    // Agrega un hijo y devuelve un puntero crudo estable para seguir construyendo el arbol.
    CstNode* add_child(std::unique_ptr<CstNode> child);

    std::string symbol;
    bool has_token = false;
    Token token{TokenType::UNKNOWN, "", 0, 0};
    std::vector<std::unique_ptr<CstNode>> children;
};

using CstNodePtr = std::unique_ptr<CstNode>;

// Utilidad de debug para imprimir el CST de forma arbolada.
std::string cst_to_string(const CstNode& node, int indent = 0);

}  // namespace parser
