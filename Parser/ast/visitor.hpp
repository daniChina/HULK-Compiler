#pragma once

namespace parser {

// Forward declarations
struct NumberExpr;
struct StringExpr;
struct NullExpr;
struct BoolExpr;
struct IdentifierExpr;
struct SelfExpr;
struct GroupedExpr;
struct UnaryExpr;
struct BinaryExpr;
struct CallExpr;
struct GetAttrExpr;
struct LetExpr;
struct BlockExpr;
struct IfExpr;
struct WhileExpr;
struct ForExpr;
struct WithExpr;
struct CaseExpr;
struct IsExpr;
struct AsExpr;
struct AssignExpr;
struct NewExpr;
struct BaseCallExpr;

struct SetAttrExpr;
struct MethodCallExpr;

// Extension expressions
struct UnlessExpr;
struct RepeatExpr;
struct LoopWhileExpr;

struct ExprStmt;
struct Program;
struct FunctionDecl;
struct ClassDecl;
struct MethodDecl;
struct AttributeDecl;

struct ExprVisitor {
    virtual ~ExprVisitor() = default;
    virtual void visit(NumberExpr* expr) = 0;
    virtual void visit(StringExpr* expr) = 0;
    virtual void visit(NullExpr* expr) = 0;
    virtual void visit(BoolExpr* expr) = 0;
    virtual void visit(IdentifierExpr* expr) = 0;
    virtual void visit(SelfExpr* expr) = 0;
    virtual void visit(GroupedExpr* expr) = 0;
    virtual void visit(UnaryExpr* expr) = 0;
    virtual void visit(BinaryExpr* expr) = 0;
    virtual void visit(CallExpr* expr) = 0;
    virtual void visit(GetAttrExpr* expr) = 0;
    virtual void visit(LetExpr* expr) = 0;
    virtual void visit(BlockExpr* expr) = 0;
    virtual void visit(IfExpr* expr) = 0;
    virtual void visit(WhileExpr* expr) = 0;
    virtual void visit(ForExpr* expr) = 0;
    virtual void visit(WithExpr* expr) = 0;
    virtual void visit(CaseExpr* expr) = 0;
    virtual void visit(IsExpr* expr) = 0;
    virtual void visit(AsExpr* expr) = 0;
    virtual void visit(AssignExpr* expr) = 0;
    virtual void visit(NewExpr* expr) = 0;
    virtual void visit(BaseCallExpr* expr) = 0;
    
    // Visitor compatibility classes
    virtual void visit(SetAttrExpr* expr) = 0;
    virtual void visit(MethodCallExpr* expr) = 0;
    
    // Extensions
    virtual void visit(UnlessExpr* expr) = 0;
    virtual void visit(RepeatExpr* expr) = 0;
    virtual void visit(LoopWhileExpr* expr) = 0;
};

struct StmtVisitor {
    virtual ~StmtVisitor() = default;
    virtual void visit(ExprStmt* stmt) = 0;
    virtual void visit(Program* stmt) = 0;
    virtual void visit(FunctionDecl* stmt) = 0;
    virtual void visit(ClassDecl* stmt) = 0;
    virtual void visit(MethodDecl* stmt) = 0;
    virtual void visit(AttributeDecl* stmt) = 0;
};

} // namespace parser
