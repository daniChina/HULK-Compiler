#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "../core/token.hpp"

namespace parser {

enum class ExprKind {
    NUMBER,
    STRING,
    NULL_VALUE,
    BOOL,
    IDENTIFIER,
    GROUPED,
    UNARY,
    BINARY,
    CALL,
    GET_ATTR,
    LET,
    BLOCK,
    IF,
    WHILE,
    FOR,
    NEW_OBJ,
    BASE_CALL
};

struct Expr {
    explicit Expr(ExprKind kind) : kind(kind) {}
    virtual ~Expr() = default;

    ExprKind kind;
};

using ExprPtr = std::unique_ptr<Expr>;

enum class StmtKind {
    EXPR,
    FUNCTION_DECL,
    TYPE_DECL
};

struct Stmt {
    explicit Stmt(StmtKind kind) : stmt_kind(kind) {}
    virtual ~Stmt() = default;

    StmtKind stmt_kind;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt final : Stmt {
    explicit ExprStmt(ExprPtr expr);

    ExprPtr expr;
};

struct FunctionDecl final : Stmt {
    FunctionDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params, std::optional<Token> return_type, ExprPtr body);

    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    std::optional<Token> return_type;
    ExprPtr body;
};

struct AttributeDef {
    Token name;
    ExprPtr value;
};

struct MethodDef {
    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    std::optional<Token> return_type;
    ExprPtr body;
};

struct TypeDecl final : Stmt {
    TypeDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params,
             std::optional<Token> parent_name, std::vector<ExprPtr> parent_args,
             std::vector<AttributeDef> attributes, std::vector<MethodDef> methods);

    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    std::optional<Token> parent_name;
    std::vector<ExprPtr> parent_args;
    std::vector<AttributeDef> attributes;
    std::vector<MethodDef> methods;
};

struct Program {
    std::vector<StmtPtr> stmts;
};

using ProgramPtr = std::unique_ptr<Program>;

std::string stmt_to_string(const Stmt& stmt);
std::string program_to_string(const Program& prog);

struct NumberExpr final : Expr {
    explicit NumberExpr(Token token);

    Token token;
};

struct StringExpr final : Expr {
    explicit StringExpr(Token token);

    Token token;
};

struct NullExpr final : Expr {
    explicit NullExpr(Token token);

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

struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, Token lparen, std::vector<ExprPtr> args);

    ExprPtr callee;
    Token lparen;
    std::vector<ExprPtr> args;
};

struct GetAttrExpr final : Expr {
    GetAttrExpr(ExprPtr object, Token dot, Token name);

    ExprPtr object;
    Token dot;
    Token name;
};

struct LetExpr final : Expr {
    LetExpr(Token name, std::optional<Token> declared_type, ExprPtr initializer, ExprPtr body);

    Token name;
    std::optional<Token> declared_type;
    ExprPtr initializer;
    ExprPtr body;
};

struct BlockExpr final : Expr {
    explicit BlockExpr(std::vector<ExprPtr> exprs);

    std::vector<ExprPtr> exprs;
};

struct IfExpr final : Expr {
    IfExpr(ExprPtr condition, ExprPtr then_branch, ExprPtr else_branch);

    ExprPtr condition;
    ExprPtr then_branch;
    ExprPtr else_branch;
};

struct WhileExpr final : Expr {
    WhileExpr(ExprPtr condition, ExprPtr body);

    ExprPtr condition;
    ExprPtr body;
};

struct ForExpr final : Expr {
    ForExpr(Token variable, ExprPtr iterable, ExprPtr body);

    Token variable;
    ExprPtr iterable;
    ExprPtr body;
};

struct NewExpr final : Expr {
    NewExpr(Token type_name, std::vector<ExprPtr> args);

    Token type_name;
    std::vector<ExprPtr> args;
};

struct BaseCallExpr final : Expr {
    BaseCallExpr(std::vector<ExprPtr> args);

    std::vector<ExprPtr> args;
};

std::string expr_to_string(const Expr& expr);

}  // namespace parser
