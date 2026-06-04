#pragma once

#include <set>
#include <string>
#include <vector>

#include "../Parser/ast/expr.hpp"
#include "../Parser/ast/visitor.hpp"
#include "../SymbolTable/symbol_table.hpp"
#include "error.hpp"

namespace semantic {

// Analizador Fase 2 sobre el AST local (`parser::`). Analizador semántico activo (R1–R4).
class Phase2Analyzer : public parser::ExprVisitor, public parser::StmtVisitor {
public:
    void analyze(parser::Program* program);

    bool hasErrors() const { return error_manager_.hasErrors(); }
    void printErrors(std::ostream& out = std::cerr) const { error_manager_.printErrors(out); }
    const std::vector<SemanticError>& getErrors() const { return error_manager_.getErrors(); }

    void visit(parser::NumberExpr* expr) override;
    void visit(parser::StringExpr* expr) override;
    void visit(parser::NullExpr* expr) override;
    void visit(parser::BoolExpr* expr) override;
    void visit(parser::IdentifierExpr* expr) override;
    void visit(parser::SelfExpr* expr) override;
    void visit(parser::GroupedExpr* expr) override;
    void visit(parser::UnaryExpr* expr) override;
    void visit(parser::BinaryExpr* expr) override;
    void visit(parser::CallExpr* expr) override;
    void visit(parser::GetAttrExpr* expr) override;
    void visit(parser::LetExpr* expr) override;
    void visit(parser::BlockExpr* expr) override;
    void visit(parser::IfExpr* expr) override;
    void visit(parser::WhileExpr* expr) override;
    void visit(parser::ForExpr* expr) override;
    void visit(parser::WithExpr* expr) override;
    void visit(parser::CaseExpr* expr) override;
    void visit(parser::IsExpr* expr) override;
    void visit(parser::AsExpr* expr) override;
    void visit(parser::AssignExpr* expr) override;
    void visit(parser::NewExpr* expr) override;
    void visit(parser::BaseCallExpr* expr) override;
    void visit(parser::SetAttrExpr* expr) override;
    void visit(parser::MethodCallExpr* expr) override;
    void visit(parser::UnlessExpr* expr) override;
    void visit(parser::RepeatExpr* expr) override;
    void visit(parser::LoopWhileExpr* expr) override;

    void visit(parser::ExprStmt* stmt) override;
    void visit(parser::Program* stmt) override;
    void visit(parser::FunctionDecl* stmt) override;
    void visit(parser::ClassDecl* stmt) override;
    void visit(parser::MethodDecl* stmt) override;
    void visit(parser::AttributeDecl* stmt) override;

private:
    SymbolTable symbol_table_;
    ErrorManager error_manager_;

    void report(ErrorType type, const std::string& message, int line, int col,
                const std::string& context = "");
    void visitExpr(parser::Expr* expr);
    void visitStmt(parser::Stmt* stmt);

    void checkUniqueParamNames(const std::vector<std::pair<parser::Token, std::optional<parser::Token>>>& params,
                               int line, int col, const std::string& context);
    void registerFunctionDecl(parser::FunctionDecl* stmt);
    void resolveVariableUse(const parser::Token& token, const std::string& context);
};

}  // namespace semantic
