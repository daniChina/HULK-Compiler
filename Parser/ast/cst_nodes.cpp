#include "cst_nodes.hpp"

#include <sstream>
#include <utility>

namespace parser {

CstNode::CstNode(std::string symbol)
    : symbol(std::move(symbol)) {}

CstNode* CstNode::add_child(std::unique_ptr<CstNode> child) {
    CstNode* raw_child = child.get();
    children.push_back(std::move(child));
    return raw_child;
}

std::string cst_to_string(const CstNode& node, int indent) {
    std::ostringstream out;
    out << std::string(static_cast<std::size_t>(indent), ' ') << node.symbol;

    // Si el nodo terminal consumio un token real, se anota el lexema para debug.
    if (node.has_token) {
        out << " => " << token_name(node.token.type) << " \"" << node.token.lexeme << "\"";
    }
    out << "\n";

    for (const auto& child : node.children) {
        out << cst_to_string(*child, indent + 2);
    }

    return out.str();
}

}  // namespace parser
