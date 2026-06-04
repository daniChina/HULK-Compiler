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
    drainPendingLetBindingErrors(error_manager_);
    collectClassDeclarations(program);
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

void Phase2Analyzer::resolveFunctionCall(const parser::Token& name_token, size_t arity,
                                         const std::string& context) {
    const std::string& name = name_token.lexeme;
    if (symbol_table_.lookupFunction(name, arity)) {
        return;
    }
    if (symbol_table_.lookupFunction(name)) {
        report(ErrorType::ARGUMENT_COUNT_MISMATCH,
               "La función '" + name + "' no acepta " + std::to_string(arity) + " argumento(s)",
               name_token.line, name_token.col, context);
        return;
    }
    report(ErrorType::UNDEFINED_FUNCTION, "Función '" + name + "' no está definida", name_token.line,
           name_token.col, context);
}

void Phase2Analyzer::registerFunctionDecl(parser::FunctionDecl* stmt) {
    checkUniqueParamNames(stmt->params, stmt->name.line, stmt->name.col, "declaración de función");

    if (symbol_table_.isBuiltinFunction(stmt->name.lexeme)) {
        report(ErrorType::REDEFINED_FUNCTION,
               "Función '" + stmt->name.lexeme + "' ya está definida", stmt->name.line,
               stmt->name.col, "declaración de función");
        return;
    }

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

bool Phase2Analyzer::isBuiltinNominalType(const std::string& name) const {
    return name == "Object" || name == "Number" || name == "String" || name == "Boolean" ||
           name == "Null" || name == "Void" || name == "Unknown";
}

void Phase2Analyzer::collectClassDeclarations(parser::Program* program) {
    for (const auto& stmt : program->stmts) {
        if (auto* class_decl = dynamic_cast<parser::ClassDecl*>(stmt.get())) {
            registerClassDecl(class_decl);
        }
    }
}

void Phase2Analyzer::registerClassDecl(parser::ClassDecl* stmt) {
    const std::string& name = stmt->name.lexeme;
    std::string base = "Object";
    if (stmt->parent_name) {
        base = stmt->parent_name->lexeme;
        if (base == "Number" || base == "String" || base == "Boolean") {
            report(ErrorType::INVALID_BASE_TYPE,
                   "No se puede heredar del tipo primitivo '" + base + "'", stmt->name.line,
                   stmt->name.col, "declaración de clase");
            return;
        }
        if (!isBuiltinNominalType(base) && !symbol_table_.isTypeDeclared(base)) {
            report(ErrorType::UNDEFINED_TYPE, "Tipo '" + base + "' no está definido",
                   stmt->parent_name->line, stmt->parent_name->col, "declaración de clase");
            return;
        }
    }

    if (!symbol_table_.declareType(name, base, stmt->name.line)) {
        report(ErrorType::REDEFINED_TYPE, "Tipo '" + name + "' ya está definido", stmt->name.line,
               stmt->name.col, "declaración de clase");
        return;
    }
    symbol_table_.storeTypeDeclaration(name, stmt);

    for (const auto& method : stmt->methods) {
        checkUniqueParamNames(method.params, method.name.line, method.name.col,
                              "declaración de método");
    }
    for (const auto& attr : stmt->attributes) {
        for (const auto& method : stmt->methods) {
            if (attr.name.lexeme == method.name.lexeme) {
                report(ErrorType::REDEFINED_ATTRIBUTE,
                       "Atributo '" + attr.name.lexeme + "' ya está definido en tipo '" + name +
                           "'",
                       attr.name.line, attr.name.col, "declaración de atributo");
            }
        }
    }
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
        resolveFunctionCall(callee_id->token, expr->args.size(), "llamada a función");
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
void Phase2Analyzer::visit(parser::IsExpr* expr) {
    visitExpr(expr->object.get());
    const std::string& type_name = expr->type_name.lexeme;
    if (!symbol_table_.isTypeDeclared(type_name)) {
        report(ErrorType::UNDEFINED_TYPE, "Tipo '" + type_name + "' no está definido",
               expr->type_name.line, expr->type_name.col, "expresión is");
    }
}

void Phase2Analyzer::visit(parser::AsExpr* expr) {
    visitExpr(expr->object.get());
    const std::string& type_name = expr->type_name.lexeme;
    if (!symbol_table_.isTypeDeclared(type_name)) {
        report(ErrorType::UNDEFINED_TYPE, "Tipo '" + type_name + "' no está definido",
               expr->type_name.line, expr->type_name.col, "expresión as");
    }
}
void Phase2Analyzer::visit(parser::AssignExpr* expr) {
    visitExpr(expr->rhs.get());

    if (auto* lhs_id = dynamic_cast<parser::IdentifierExpr*>(expr->lhs.get())) {
        resolveVariableUse(lhs_id->token, "asignación");
    } else {
        visitExpr(expr->lhs.get());
    }
}
void Phase2Analyzer::visit(parser::NewExpr* expr) {
    const std::string& type_name = expr->type_name.lexeme;
    if (!symbol_table_.isTypeDeclared(type_name)) {
        report(ErrorType::UNDEFINED_TYPE, "Tipo '" + type_name + "' no está definido",
               expr->type_name.line, expr->type_name.col, "instanciación de objeto");
    } else {
        auto* class_decl = symbol_table_.getTypeDeclaration(type_name);
        if (class_decl && expr->args.size() != class_decl->params.size()) {
            report(ErrorType::ARGUMENT_COUNT_MISMATCH,
                   "El constructor de '" + type_name + "' espera " +
                       std::to_string(class_decl->params.size()) + " parámetro(s) pero se recibieron " +
                       std::to_string(expr->args.size()),
                   expr->type_name.line, expr->type_name.col, "instanciación de objeto");
        }
    }
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
