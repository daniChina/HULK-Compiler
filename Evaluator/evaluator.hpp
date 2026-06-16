#pragma once

#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "../Parser/ast/expr.hpp"
#include "../Value/value.hpp"
#include "env_frame.hpp"
#include "user_function.hpp"

namespace eval {

// Fase 3 — intérprete sobre AST local .
class Evaluator : public parser::ExprVisitor, public parser::StmtVisitor {
public:
    explicit Evaluator(std::ostream& out = std::cout);

    value::Value evaluate(parser::Program* program);

    bool hadError() const { return had_error_; }
    const std::string& lastError() const { return last_error_; }

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
    std::ostream& out_;
    std::shared_ptr<EnvFrame> global_;
    std::map<std::string, FunctionOverloadMap> functions_;
    std::map<std::string, parser::ClassDecl*> types_;
    std::shared_ptr<value::Instance> current_self_;
    std::string current_method_name_;
    value::Value current_{0.0};
    bool had_error_ = false;
    std::string last_error_;

    void visitExpr(parser::Expr* expr);
    void visitStmt(parser::Stmt* stmt);
    void setError(const std::string& message);

    static bool isBuiltinFunctionName(const std::string& name);
    void registerFunction(parser::FunctionDecl* decl);
    void registerClass(parser::ClassDecl* decl);
    value::Value constructInstance(parser::ClassDecl* type_def, const std::vector<parser::ExprPtr>& args);
    void initializeParentAttributes(value::Instance& instance, parser::ClassDecl* type_def);
    void initializeAttributes(value::Instance& instance, parser::ClassDecl* type_def);
    std::shared_ptr<value::Instance> resolveInstance(parser::Expr* object_expr);
    const parser::MethodDef* findMethod(parser::ClassDecl* type_def, const std::string& method_name) const;
    void invokeMethod(const std::shared_ptr<value::Instance>& instance, const parser::MethodDef& method,
                      const std::vector<value::Value>& args);
    bool assignInstanceAttr(const std::shared_ptr<value::Instance>& instance, const std::string& attr_name,
                            const value::Value& value);
    bool instanceConformsTo(parser::ClassDecl* dynamic_type, const std::string& static_type_name) const;
    void invokeUserFunction(const UserFunction& fn, const std::vector<parser::ExprPtr>& args);
    bool callBuiltin(const std::string& name, const std::vector<parser::ExprPtr>& args);
};

}  // namespace eval
