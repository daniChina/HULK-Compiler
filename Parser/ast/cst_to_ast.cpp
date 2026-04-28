#include "cst_to_ast.hpp"

#include <stdexcept>
#include <utility>

#include "../generator/production.hpp"

namespace parser {
namespace {

// Verifica la forma estructural esperada del CST antes de convertirla a AST.
void expect_symbol(const CstNode& node, const std::string& expected) {
    if (node.symbol != expected) {
        throw std::runtime_error(
            "Se esperaba nodo CST " + expected + " pero se encontro " + node.symbol);
    }
}

// Acceso corto para no repetir desreferenciaciones sobre `children`.
const CstNode& child(const CstNode& node, std::size_t index) {
    if (index >= node.children.size()) {
        throw std::runtime_error(
            "Indice de hijo fuera de rango en nodo CST " + node.symbol);
    }

    return *node.children[index];
}

bool is_epsilon_node(const CstNode& node) {
    return node.symbol == generator::kEpsilonSymbol;
}

ExprPtr build_expr(const CstNode& node);
ExprPtr build_or_expr(const CstNode& node);
ExprPtr build_or_tail(ExprPtr left, const CstNode& node);
ExprPtr build_and_expr(const CstNode& node);
ExprPtr build_and_tail(ExprPtr left, const CstNode& node);
ExprPtr build_cmp_expr(const CstNode& node);
ExprPtr build_cmp_tail(ExprPtr left, const CstNode& node);
ExprPtr build_concat_expr(const CstNode& node);
ExprPtr build_concat_tail(ExprPtr left, const CstNode& node);
ExprPtr build_add_expr(const CstNode& node);
ExprPtr build_add_tail(ExprPtr left, const CstNode& node);
ExprPtr build_mul_expr(const CstNode& node);
ExprPtr build_mul_tail(ExprPtr left, const CstNode& node);
ExprPtr build_power_expr(const CstNode& node);
ExprPtr build_power_tail(ExprPtr left, const CstNode& node);
ExprPtr build_unary_expr(const CstNode& node);
ExprPtr build_postfix_expr(const CstNode& node);
ExprPtr build_postfix_tail(ExprPtr left, const CstNode& node);
ExprPtr build_primary(const CstNode& node);

ExprPtr build_expr_stmt(const CstNode& node) {
    expect_symbol(node, "ExprStmt");
    return build_expr(child(node, 0));
}

ExprPtr build_expr(const CstNode& node) {
    expect_symbol(node, "Expr");
    return build_or_expr(child(node, 0));
}

ExprPtr build_or_expr(const CstNode& node) {
    expect_symbol(node, "OrExpr");
    auto left = build_and_expr(child(node, 0));
    return build_or_tail(std::move(left), child(node, 1));
}

ExprPtr build_or_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "OrExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_and_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_or_tail(std::move(combined), child(node, 2));
}

ExprPtr build_and_expr(const CstNode& node) {
    expect_symbol(node, "AndExpr");
    auto left = build_cmp_expr(child(node, 0));
    return build_and_tail(std::move(left), child(node, 1));
}

ExprPtr build_and_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "AndExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_cmp_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_and_tail(std::move(combined), child(node, 2));
}

ExprPtr build_cmp_expr(const CstNode& node) {
    expect_symbol(node, "CmpExpr");
    auto left = build_concat_expr(child(node, 0));
    return build_cmp_tail(std::move(left), child(node, 1));
}

ExprPtr build_cmp_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "CmpExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_concat_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_cmp_tail(std::move(combined), child(node, 2));
}

ExprPtr build_concat_expr(const CstNode& node) {
    expect_symbol(node, "ConcatExpr");
    auto left = build_add_expr(child(node, 0));
    return build_concat_tail(std::move(left), child(node, 1));
}

ExprPtr build_concat_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "ConcatExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_add_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_concat_tail(std::move(combined), child(node, 2));
}

ExprPtr build_add_expr(const CstNode& node) {
    expect_symbol(node, "AddExpr");
    auto left = build_mul_expr(child(node, 0));
    return build_add_tail(std::move(left), child(node, 1));
}

ExprPtr build_add_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "AddExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_mul_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_add_tail(std::move(combined), child(node, 2));
}

ExprPtr build_mul_expr(const CstNode& node) {
    expect_symbol(node, "MulExpr");
    auto left = build_power_expr(child(node, 0));
    return build_mul_tail(std::move(left), child(node, 1));
}

ExprPtr build_mul_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "MulExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_power_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_mul_tail(std::move(combined), child(node, 2));
}

ExprPtr build_power_expr(const CstNode& node) {
    expect_symbol(node, "PowerExpr");
    auto left = build_unary_expr(child(node, 0));
    return build_power_tail(std::move(left), child(node, 1));
}

ExprPtr build_power_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "PowerExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    // Aqui se conserva la asociatividad derecha: el lado derecho vuelve a ser PowerExpr completo.
    Token op = child(node, 0).token;
    auto right = build_power_expr(child(node, 1));
    return std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
}

ExprPtr build_unary_expr(const CstNode& node) {
    expect_symbol(node, "UnaryExpr");

    if (node.children.size() == 1) {
        const auto& only_child = child(node, 0);
        if (only_child.symbol == "PostfixExpr") {
            return build_postfix_expr(only_child);
        }
        return build_primary(only_child);
    }

    Token op = child(node, 0).token;
    auto right = build_unary_expr(child(node, 1));
    return std::make_unique<UnaryExpr>(std::move(op), std::move(right));
}

ExprPtr build_postfix_expr(const CstNode& node) {
    expect_symbol(node, "PostfixExpr");
    auto left = build_primary(child(node, 0));
    return build_postfix_tail(std::move(left), child(node, 1));
}

ExprPtr build_postfix_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "PostfixTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    // La fase actual solo necesitaba abrir el nivel PostfixExpr sin romper el AST existente.
    // Los nodos AST de llamadas/acceso de miembro se agregan en el siguiente paso de iteracion 1.
    throw std::runtime_error(
        "Conversion CST -> AST para sufijos postfix aun no implementada");
}

ExprPtr build_primary(const CstNode& node) {
    expect_symbol(node, "Primary");

    if (node.children.size() == 1) {
        const auto& value = child(node, 0);

        if (value.symbol == "NUMBER_LITERAL") {
            return std::make_unique<NumberExpr>(value.token);
        }
        if (value.symbol == "STRING_LITERAL") {
            return std::make_unique<StringExpr>(value.token);
        }
        if (value.symbol == "TRUE") {
            return std::make_unique<BoolExpr>(value.token, true);
        }
        if (value.symbol == "FALSE") {
            return std::make_unique<BoolExpr>(value.token, false);
        }
        if (value.symbol == "IDENTIFIER") {
            return std::make_unique<IdentifierExpr>(value.token);
        }
    }

    // Caso agrupado: LPAREN Expr RPAREN
    if (node.children.size() == 3 && child(node, 0).symbol == "LPAREN") {
        Token lparen = child(node, 0).token;
        auto expression = build_expr(child(node, 1));
        return std::make_unique<GroupedExpr>(std::move(lparen), std::move(expression));
    }

    throw std::runtime_error("Forma de Primary no soportada en la conversion CST -> AST");
}

}  // namespace

ExprPtr cst_to_ast(const CstNode& root) {
    expect_symbol(root, "Program");
    return build_expr_stmt(child(root, 0));
}

}  // namespace parser
