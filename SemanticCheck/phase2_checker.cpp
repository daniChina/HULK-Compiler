#include "phase2_checker.hpp"

#include "../Types/type_info.hpp"
#include <vector>

namespace semantic {

void Phase2Analyzer::report(ErrorType type, const std::string& message, int line, int col,
                            const std::string& context) {
    error_manager_.reportError(type, message, line, col, context, "Phase2Analyzer");
}

void Phase2Analyzer::visitExpr(parser::Expr* expr) {
    if (expr) {
        expr->accept(this);
    }
}

void Phase2Analyzer::visitStmt(parser::Stmt* stmt) {
    if (stmt) {
        stmt->accept(this);
    }
}

void Phase2Analyzer::analyze(parser::Program* program) {
    if (!program) {
        return;
    }
    TypeInfo::setSymbolTable(&symbol_table_);
    program->accept(this);
}

void Phase2Analyzer::checkUniqueParamNames(
    const std::vector<std::pair<parser::Token, std::optional<parser::Token>>>& params, int line,
    int col, const std::string& context) {
    std::set<std::string> seen;
    for (const auto& param : params) {
        if (!seen.insert(param.first.lexeme).second) {
            report(ErrorType::REDEFINED_VARIABLE,
                   "Variable '" + param.first.lexeme + "' ya está definida en este ámbito",
                   param.first.line, param.first.col, context);
        }
    }
}

void Phase2Analyzer::resolveVariableUse(const parser::Token& token, const std::string& context) {
    if (!symbol_table_.lookupVariable(token.lexeme)) {
        report(ErrorType::UNDEFINED_VARIABLE,
               "Variable '" + token.lexeme + "' no está definida", token.line, token.col, context);
    }
}

void Phase2Analyzer::registerFunctionDecl(parser::FunctionDecl* stmt) {
    checkUniqueParamNames(stmt->params, stmt->name.line, stmt->name.col, "declaración de función");

    std::vector<TypeInfo> param_types;
    param_types.reserve(stmt->params.size());
    for (size_t i = 0; i < stmt->params.size(); ++i) {
        param_types.push_back(TypeInfo(TypeInfo::Kind::Unknown));
    }

    if (!symbol_table_.declareFunction(stmt->name.lexeme, param_types,
                                       TypeInfo(TypeInfo::Kind::Unknown), stmt->name.line)) {
        report(ErrorType::REDEFINED_FUNCTION,
               "Función '" + stmt->name.lexeme + "' ya está definida", stmt->name.line,
               stmt->name.col, "declaración de función");
    }
}

void Phase2Analyzer::visit(parser::Program* program) {
    for (const auto& stmt : program->stmts) {
        visitStmt(stmt.get());
    }
}

void Phase2Analyzer::visit(parser::ExprStmt* stmt) {
    visitExpr(stmt->expr.get());
}

void Phase2Analyzer::visit(parser::FunctionDecl* stmt) {
    registerFunctionDecl(stmt);

    symbol_table_.enterScope();
    for (const auto& param : stmt->params) {
        symbol_table_.declareVariable(param.first.lexeme, TypeInfo(TypeInfo::Kind::Unknown));
    }
    visitExpr(stmt->body.get());
    symbol_table_.exitScope();
}

void Phase2Analyzer::visit(parser::ClassDecl* stmt) {
    for (const auto& method : stmt->methods) {
        checkUniqueParamNames(method.params, method.name.line, method.name.col,
                              "declaración de método");
    }
    (void)stmt;
}

void Phase2Analyzer::visit(parser::MethodDecl* stmt) {
    checkUniqueParamNames(stmt->params, stmt->name.line, stmt->name.col, "declaración de método");
    (void)stmt;
}

void Phase2Analyzer::visit(parser::AttributeDecl* stmt) { (void)stmt; }

void Phase2Analyzer::visit(parser::NumberExpr* expr) { (void)expr; }
void Phase2Analyzer::visit(parser::StringExpr* expr) { (void)expr; }
void Phase2Analyzer::visit(parser::NullExpr* expr) { (void)expr; }
void Phase2Analyzer::visit(parser::BoolExpr* expr) { (void)expr; }
void Phase2Analyzer::visit(parser::SelfExpr* expr) { (void)expr; }
void Phase2Analyzer::visit(parser::GroupedExpr* expr) { visitExpr(expr->expression.get()); }
void Phase2Analyzer::visit(parser::UnaryExpr* expr) { visitExpr(expr->right.get()); }
void Phase2Analyzer::visit(parser::BinaryExpr* expr) {
    visitExpr(expr->left.get());
    visitExpr(expr->right.get());
}

void Phase2Analyzer::visit(parser::CallExpr* expr) {
    if (auto* callee_id = dynamic_cast<parser::IdentifierExpr*>(expr->callee.get())) {
        const std::string& name = callee_id->token.lexeme;
        if (!symbol_table_.lookupFunction(name, expr->args.size())) {
            report(ErrorType::UNDEFINED_FUNCTION, "Función '" + name + "' no está definida",
                   callee_id->token.line, callee_id->token.col, "llamada a función");
        }
    } else {
        visitExpr(expr->callee.get());
    }

    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
    }
}

void Phase2Analyzer::visit(parser::GetAttrExpr* expr) { visitExpr(expr->object.get()); }
void Phase2Analyzer::visit(parser::IfExpr* expr) {
    visitExpr(expr->condition.get());
    visitExpr(expr->then_branch.get());
    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    }
}
void Phase2Analyzer::visit(parser::WhileExpr* expr) {
    visitExpr(expr->condition.get());
    visitExpr(expr->body.get());
    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    }
}
void Phase2Analyzer::visit(parser::ForExpr* expr) {
    visitExpr(expr->iterable.get());

    symbol_table_.enterScope();
    symbol_table_.declareVariable(expr->variable.lexeme, TypeInfo(TypeInfo::Kind::Unknown));
    visitExpr(expr->body.get());
    symbol_table_.exitScope();
}

void Phase2Analyzer::visit(parser::WithExpr* expr) {
    visitExpr(expr->value.get());

    symbol_table_.enterScope();
    symbol_table_.declareVariable(expr->alias.lexeme, TypeInfo(TypeInfo::Kind::Unknown));
    visitExpr(expr->body.get());
    symbol_table_.exitScope();

    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    }
}
void Phase2Analyzer::visit(parser::CaseExpr* expr) {
    visitExpr(expr->value.get());
    for (const auto& branch : expr->branches) {
        visitExpr(branch.body.get());
    }
}
void Phase2Analyzer::visit(parser::IsExpr* expr) { visitExpr(expr->object.get()); }
void Phase2Analyzer::visit(parser::AsExpr* expr) { visitExpr(expr->object.get()); }
void Phase2Analyzer::visit(parser::AssignExpr* expr) {
    visitExpr(expr->rhs.get());

    if (auto* lhs_id = dynamic_cast<parser::IdentifierExpr*>(expr->lhs.get())) {
        resolveVariableUse(lhs_id->token, "asignación");
    } else {
        visitExpr(expr->lhs.get());
    }
}
void Phase2Analyzer::visit(parser::NewExpr* expr) {
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
    }
}
void Phase2Analyzer::visit(parser::BaseCallExpr* expr) {
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
    }
}
void Phase2Analyzer::visit(parser::SetAttrExpr* expr) {
    visitExpr(expr->object.get());
    visitExpr(expr->value.get());
}
void Phase2Analyzer::visit(parser::MethodCallExpr* expr) {
    visitExpr(expr->object.get());
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
    }
}
void Phase2Analyzer::visit(parser::UnlessExpr* expr) {
    visitExpr(expr->condition.get());
    visitExpr(expr->then_branch.get());
    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    }
}
void Phase2Analyzer::visit(parser::RepeatExpr* expr) {
    visitExpr(expr->count.get());
    visitExpr(expr->body.get());
}
void Phase2Analyzer::visit(parser::LoopWhileExpr* expr) {
    visitExpr(expr->body.get());
    visitExpr(expr->condition.get());
}

void Phase2Analyzer::visit(parser::BlockExpr* expr) {
    for (const auto& sub : expr->exprs) {
        visitExpr(sub.get());
    }
}

void Phase2Analyzer::visit(parser::LetExpr* expr) {
    visitExpr(expr->initializer.get());

    symbol_table_.enterScope();

    if (symbol_table_.isBuiltinVariable(expr->name.lexeme)) {
        report(ErrorType::REDEFINED_VARIABLE,
               "Variable '" + expr->name.lexeme + "' ya está definida en este ámbito",
               expr->name.line, expr->name.col, "expresión let");
    } else if (!symbol_table_.declareVariable(expr->name.lexeme, TypeInfo(TypeInfo::Kind::Unknown))) {
        report(ErrorType::REDEFINED_VARIABLE,
               "Variable '" + expr->name.lexeme + "' ya está definida en este ámbito",
               expr->name.line,
               expr->name.col,
               "expresión let");
    }

    visitExpr(expr->body.get());
    symbol_table_.exitScope();
}

void Phase2Analyzer::visit(parser::IdentifierExpr* expr) {
    resolveVariableUse(expr->token, "identificador");
}

}  // namespace semantic
