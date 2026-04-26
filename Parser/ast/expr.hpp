#pragma once

#include <memory>
#include <string>

#include "../core/token.hpp"

namespace parser {

enum class ExprKind {
    NUMBER,
    STRING,
    BOOL,
    IDENTIFIER,
    GROUPED,
    UNARY,
    BINARY
};

struct Expr {
    explicit Expr(ExprKind kind) : kind(kind) {}
    virtual ~Expr() = default;

    ExprKind kind;
};

using ExprPtr = std::unique_ptr<Expr>;

struct NumberExpr final : Expr {
    explicit NumberExpr(Token token);

    Token token;
};

struct StringExpr final : Expr {
    explicit StringExpr(Token token);

    Token token;
};

struct BoolExpr final : Expr {
    BoolExpr(Token token, bool value);

    Token token;
    bool value;
};

struct IdentifierExpr final : Expr {
    explicit IdentifierExpr(Token token);

    Token token;
};

struct GroupedExpr final : Expr {
    GroupedExpr(Token lparen, ExprPtr expression);

    Token lparen;
    ExprPtr expression;
};

struct UnaryExpr final : Expr {
    UnaryExpr(Token op, ExprPtr right);

    Token op;
    ExprPtr right;
};

struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr left, Token op, ExprPtr right);

    ExprPtr left;
    Token op;
    ExprPtr right;
};

std::string expr_to_string(const Expr& expr);

}  // namespace parser
