#pragma once

#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "../Parser/ast/expr.hpp"
#include "../Parser/ast/visitor.hpp"
#include "../SymbolTable/symbol_table.hpp"
#include "binding_list.hpp"
#include "error.hpp"
#include "type_map.hpp"

namespace semantic {

// Analizador Fase 2 sobre el AST local (`parser::`). Analizador semántico activo (R1–R4).
// Pasadas: (1) collect; (2) clases; (3) fixed-point; (4) override retorno; TypeMap en cada visitExpr (I11).
class Phase2Analyzer : public parser::ExprVisitor, public parser::StmtVisitor {
public:
    Phase2Analyzer() : current_type_(TypeInfo::Kind::Unknown), current_class_("") {}

    void analyze(parser::Program* program);

    const TypeMap& getTypeMap() const { return type_map_; }

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
    void resolveFunctionCall(const parser::Token& name_token, size_t arity, const std::string& context);

    void collectClassDeclarations(parser::Program* program);
    void collectFunctionDeclarations(parser::Program* program);
    void registerClassDecl(parser::ClassDecl* stmt);
    bool validateInheritanceChain(const std::string& class_name, const std::string& base_name,
                                  int line, int col);
    bool isBuiltinNominalType(const std::string& name) const;
    bool validateTypeExists(const std::optional<parser::Token>& token);
    void propagateTypeToExpr(parser::Expr* expr, const TypeInfo& type);
    void propagateNumericPair(parser::Expr* left, parser::Expr* right, TypeInfo& left_type,
                              TypeInfo& right_type);
    void propagateStringPair(parser::Expr* left, parser::Expr* right, TypeInfo& left_type,
                             TypeInfo& right_type);
    void propagateBooleanPair(parser::Expr* left, parser::Expr* right, TypeInfo& left_type,
                              TypeInfo& right_type);
    bool analyzeFunctionDecl(parser::FunctionDecl* stmt);
    bool analyzeClassMethod(parser::ClassDecl* class_decl, const parser::MethodDef& method);
    void runInferencePasses(parser::Program* program);
    void validateMethodOverrideReturns(parser::Program* program);
    std::optional<std::pair<std::shared_ptr<FunctionSymbol>, std::string>>
    lookupInheritedMethod(const std::string& class_name, const std::string& method_name);
    bool methodSignatureMatchesOverride(const parser::MethodDef& method,
                                        const FunctionSymbol& inherited);

    std::map<std::string, std::string> pending_class_parents_;
    TypeMap type_map_;
    TypeInfo current_type_;
    std::string current_class_;
    TypeInfo resolveType(const std::optional<parser::Token>& token);
};

}  // namespace semantic
