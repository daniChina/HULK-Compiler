#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>

#include "../core/token.hpp"
#include "visitor.hpp"

namespace parser {

enum class ExprKind {
    NUMBER,
    STRING,
    NULL_VALUE,
    BOOL,
    IDENTIFIER,
    SELF_REF,
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
    WITH,
    CASE_EXPR,
    IS_EXPR,
    AS_EXPR,
    ASSIGN_EXPR,
    NEW_OBJ,
    BASE_CALL,
    UNLESS_EXPR,
    REPEAT_EXPR,
    LOOP_WHILE_EXPR
};

struct Expr {
    explicit Expr(ExprKind kind) : kind(kind) {}
    virtual ~Expr() = default;

    virtual void accept(ExprVisitor* visitor) = 0;

    ExprKind kind;
};

using ExprPtr = std::unique_ptr<Expr>;

enum class StmtKind {
    EXPR,
    FUNCTION_DECL,
    CLASS_DECL,
    PROGRAM,
    METHOD_DECL,
    ATTRIBUTE_DECL
};

struct Stmt {
    explicit Stmt(StmtKind kind) : stmt_kind(kind) {}
    virtual ~Stmt() = default;

    virtual void accept(StmtVisitor* visitor) = 0;

    StmtKind stmt_kind;
};

using StmtPtr = std::unique_ptr<Stmt>;

struct ExprStmt final : Stmt {
    explicit ExprStmt(ExprPtr expr);
    void accept(StmtVisitor* visitor) override;

    ExprPtr expr;
};

struct FunctionDecl final : Stmt {
    FunctionDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params, std::optional<Token> return_type, ExprPtr body);
    void accept(StmtVisitor* visitor) override;

    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    std::optional<Token> return_type;
    ExprPtr body;
};

struct AttributeDef {
    Token name;
    std::optional<Token> declared_type;
    ExprPtr value;
};

struct MethodDef {
    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    ExprPtr body;
};

struct ClassDecl final : Stmt {
    ClassDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params,
              std::optional<Token> parent_name, std::vector<ExprPtr> parent_args,
              std::vector<AttributeDef> attributes, std::vector<MethodDef> methods);
    void accept(StmtVisitor* visitor) override;

    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    std::optional<Token> parent_name;
    std::vector<ExprPtr> parent_args;
    std::vector<AttributeDef> attributes;
    std::vector<MethodDef> methods;
};

struct AttributeDecl final : Stmt {
    AttributeDecl(Token name, ExprPtr initializer, std::optional<Token> declared_type);
    void accept(StmtVisitor* visitor) override;

    Token name;
    ExprPtr initializer;
    std::optional<Token> declared_type;
};

struct MethodDecl final : Stmt {
    MethodDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params, std::optional<Token> return_type, ExprPtr body);
    void accept(StmtVisitor* visitor) override;

    Token name;
    std::vector<std::pair<Token, std::optional<Token>>> params;
    std::optional<Token> return_type;
    ExprPtr body;
};

struct Program final : Stmt {
    explicit Program(std::vector<StmtPtr> stmts);
    void accept(StmtVisitor* visitor) override;

    std::vector<StmtPtr> stmts;
};

using ProgramPtr = std::unique_ptr<Program>;

std::string stmt_to_string(const Stmt& stmt);
std::string program_to_string(const Program& prog);

struct NumberExpr final : Expr {
    explicit NumberExpr(Token token);
    void accept(ExprVisitor* visitor) override;

    Token token;
};

struct StringExpr final : Expr {
    explicit StringExpr(Token token);
    void accept(ExprVisitor* visitor) override;

    Token token;
};

struct NullExpr final : Expr {
    explicit NullExpr(Token token);
    void accept(ExprVisitor* visitor) override;

    Token token;
};

struct BoolExpr final : Expr {
    BoolExpr(Token token, bool value);
    void accept(ExprVisitor* visitor) override;

    Token token;
    bool value;
};

struct IdentifierExpr final : Expr {
    explicit IdentifierExpr(Token token);
    void accept(ExprVisitor* visitor) override;

    Token token;
};

struct SelfExpr final : Expr {
    explicit SelfExpr(Token token);
    void accept(ExprVisitor* visitor) override;

    Token token;
};

struct GroupedExpr final : Expr {
    GroupedExpr(Token lparen, ExprPtr expression);
    void accept(ExprVisitor* visitor) override;

    Token lparen;
    ExprPtr expression;
};

struct UnaryExpr final : Expr {
    UnaryExpr(Token op, ExprPtr right);
    void accept(ExprVisitor* visitor) override;

    Token op;
    ExprPtr right;
};

struct BinaryExpr final : Expr {
    BinaryExpr(ExprPtr left, Token op, ExprPtr right);
    void accept(ExprVisitor* visitor) override;

    ExprPtr left;
    Token op;
    ExprPtr right;
};

struct CallExpr final : Expr {
    CallExpr(ExprPtr callee, Token lparen, std::vector<ExprPtr> args);
    void accept(ExprVisitor* visitor) override;

    ExprPtr callee;
    Token lparen;
    std::vector<ExprPtr> args;
};

struct GetAttrExpr final : Expr {
    GetAttrExpr(ExprPtr object, Token dot, Token name);
    void accept(ExprVisitor* visitor) override;

    ExprPtr object;
    Token dot;
    Token name;
};

struct LetExpr final : Expr {
    LetExpr(Token name, std::optional<Token> declared_type, ExprPtr initializer, ExprPtr body);
    void accept(ExprVisitor* visitor) override;

    Token name;
    std::optional<Token> declared_type;
    ExprPtr initializer;
    ExprPtr body;
};

struct BlockExpr final : Expr {
    explicit BlockExpr(std::vector<ExprPtr> exprs);
    void accept(ExprVisitor* visitor) override;

    std::vector<ExprPtr> exprs;
};

struct IfExpr final : Expr {
    IfExpr(ExprPtr condition, ExprPtr then_branch, ExprPtr else_branch);
    void accept(ExprVisitor* visitor) override;

    ExprPtr condition;
    ExprPtr then_branch;
    ExprPtr else_branch;
};

struct WhileExpr final : Expr {
    WhileExpr(ExprPtr condition, ExprPtr body, ExprPtr else_branch);
    void accept(ExprVisitor* visitor) override;

    ExprPtr condition;
    ExprPtr body;
    ExprPtr else_branch;
};

struct ForExpr final : Expr {
    ForExpr(Token variable, std::optional<Token> declared_type, ExprPtr iterable, ExprPtr body);
    void accept(ExprVisitor* visitor) override;

    Token variable;
    std::optional<Token> declared_type;
    ExprPtr iterable;
    ExprPtr body;
};

struct WithExpr final : Expr {
    WithExpr(ExprPtr value, Token alias, ExprPtr body, ExprPtr else_branch);
    void accept(ExprVisitor* visitor) override;

    ExprPtr value;
    Token alias;
    ExprPtr body;
    ExprPtr else_branch;
};

struct CaseBranchDef {
    Token name;
    Token type_name;
    ExprPtr body;
};

struct CaseExpr final : Expr {
    CaseExpr(ExprPtr value, std::vector<CaseBranchDef> branches);
    void accept(ExprVisitor* visitor) override;

    ExprPtr value;
    std::vector<CaseBranchDef> branches;
};

struct IsExpr final : Expr {
    IsExpr(ExprPtr object, Token is_keyword, Token type_name);
    void accept(ExprVisitor* visitor) override;

    ExprPtr object;
    Token is_keyword;
    Token type_name;
};

struct AsExpr final : Expr {
    AsExpr(ExprPtr object, Token as_keyword, Token type_name);
    void accept(ExprVisitor* visitor) override;

    ExprPtr object;
    Token as_keyword;
    Token type_name;
};

struct AssignExpr final : Expr {
    AssignExpr(ExprPtr lhs, Token op, ExprPtr rhs);
    void accept(ExprVisitor* visitor) override;

    ExprPtr lhs;
    Token op;
    ExprPtr rhs;
};

struct NewExpr final : Expr {
    NewExpr(Token type_name, std::vector<ExprPtr> args);
    void accept(ExprVisitor* visitor) override;

    Token type_name;
    std::vector<ExprPtr> args;
};

struct BaseCallExpr final : Expr {
    BaseCallExpr(std::vector<ExprPtr> args);
    void accept(ExprVisitor* visitor) override;

    std::vector<ExprPtr> args;
};

struct SetAttrExpr final : Expr {
    SetAttrExpr(ExprPtr object, Token attr_name, ExprPtr value);
    void accept(ExprVisitor* visitor) override;

    ExprPtr object;
    Token attr_name;
    ExprPtr value;
};

struct MethodCallExpr final : Expr {
    MethodCallExpr(ExprPtr object, Token method_name, std::vector<ExprPtr> args);
    void accept(ExprVisitor* visitor) override;

    ExprPtr object;
    Token method_name;
    std::vector<ExprPtr> args;
};

// Extension expressions
struct UnlessExpr final : Expr {
    UnlessExpr(ExprPtr condition, ExprPtr then_branch, ExprPtr else_branch);
    void accept(ExprVisitor* visitor) override;

    ExprPtr condition;
    ExprPtr then_branch;
    ExprPtr else_branch;
};

struct RepeatExpr final : Expr {
    RepeatExpr(ExprPtr count, ExprPtr body);
    void accept(ExprVisitor* visitor) override;

    ExprPtr count;
    ExprPtr body;
};

struct LoopWhileExpr final : Expr {
    LoopWhileExpr(ExprPtr body, ExprPtr condition);
    void accept(ExprVisitor* visitor) override;

    ExprPtr body;
    ExprPtr condition;
};

std::string expr_to_string(const Expr& expr);

}  // namespace parser
