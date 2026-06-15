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
    collectFunctionDeclarations(program);
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
    for (const auto& p : stmt->params) {
        validateTypeExists(p.second);
        param_types.push_back(resolveType(p.second));
    }
    validateTypeExists(stmt->return_type);
    TypeInfo ret_type = resolveType(stmt->return_type);

    if (!symbol_table_.declareFunction(stmt->name.lexeme, param_types,
                                       ret_type, stmt->name.line)) {
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
    symbol_table_.enterScope();
    for (const auto& param : stmt->params) {
        validateTypeExists(param.second);
        symbol_table_.declareVariable(param.first.lexeme, resolveType(param.second));
    }
    visitExpr(stmt->body.get());
    TypeInfo body_type = current_type_;
    symbol_table_.exitScope();

    if (stmt->return_type.has_value()) {
        validateTypeExists(stmt->return_type);
        TypeInfo ret_type = resolveType(stmt->return_type);
        if (!body_type.conformsTo(ret_type)) {
            report(ErrorType::TYPE_ERROR,
                   "El tipo de retorno de la función '" + body_type.toString() +
                       "' no conforma al tipo de retorno declarado '" + ret_type.toString() + "'",
                   stmt->name.line, stmt->name.col, "declaración de función");
        }
    } else {
        auto func = symbol_table_.lookupFunction(stmt->name.lexeme, stmt->params.size());
        if (func && func->return_type.getKind() == TypeInfo::Kind::Unknown) {
            func->return_type = body_type;
        }
    }
}

bool Phase2Analyzer::isBuiltinNominalType(const std::string& name) const {
    return name == "Object" || name == "Number" || name == "String" || name == "Boolean" ||
           name == "Null" || name == "Void" || name == "Unknown";
}

bool Phase2Analyzer::validateTypeExists(const std::optional<parser::Token>& token) {
    if (!token.has_value()) return true;
    const std::string& name = token->lexeme;
    if (isBuiltinNominalType(name)) return true;
    if (!symbol_table_.isTypeDeclared(name)) {
        report(ErrorType::UNDEFINED_TYPE, "Tipo '" + name + "' no está definido", token->line, token->col, "anotación de tipo");
        return false;
    }
    return true;
}

void Phase2Analyzer::collectClassDeclarations(parser::Program* program) {
    for (const auto& stmt : program->stmts) {
        if (auto* class_decl = dynamic_cast<parser::ClassDecl*>(stmt.get())) {
            registerClassDecl(class_decl);
        }
    }
}

void Phase2Analyzer::collectFunctionDeclarations(parser::Program* program) {
    for (const auto& stmt : program->stmts) {
        if (auto* fn = dynamic_cast<parser::FunctionDecl*>(stmt.get())) {
            registerFunctionDecl(fn);
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

    // Register methods and attributes in type symbol
    for (const auto& param : stmt->params) {
        validateTypeExists(param.second);
    }
    for (const auto& attr : stmt->attributes) {
        validateTypeExists(attr.declared_type);
        TypeInfo attr_type = resolveType(attr.declared_type);
        symbol_table_.addTypeAttribute(name, attr.name.lexeme, attr_type, attr.name.line);
    }
    for (const auto& method : stmt->methods) {
        std::vector<TypeInfo> method_params;
        for (const auto& p : method.params) {
            validateTypeExists(p.second);
            method_params.push_back(resolveType(p.second));
        }
        symbol_table_.addTypeMethod(name, method.name.lexeme, method_params, TypeInfo(TypeInfo::Kind::Unknown), method.name.line);
    }

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

TypeInfo Phase2Analyzer::resolveType(const std::optional<parser::Token>& token) {
    if (!token.has_value()) {
        return TypeInfo(TypeInfo::Kind::Unknown);
    }
    const std::string& name = token->lexeme;
    if (name == "Number") return TypeInfo(TypeInfo::Kind::Number);
    if (name == "String") return TypeInfo(TypeInfo::Kind::String);
    if (name == "Boolean") return TypeInfo(TypeInfo::Kind::Boolean);
    if (name == "Object") return TypeInfo(TypeInfo::Kind::Object);
    if (name == "Void") return TypeInfo(TypeInfo::Kind::Void);
    return TypeInfo(TypeInfo::Kind::Object, name);
}

void Phase2Analyzer::visit(parser::ClassDecl* stmt) {
    std::string base = stmt->parent_name ? stmt->parent_name->lexeme : "Object";
    for (const auto& method : stmt->methods) {
        checkUniqueParamNames(method.params, method.name.line, method.name.col,
                               "declaración de método");
        if (base != "Object") {
            auto base_method = symbol_table_.lookupMethod(base, method.name.lexeme);
            if (base_method) {
                bool match = (method.params.size() == base_method->parameter_types.size());
                if (match) {
                    for (size_t i = 0; i < method.params.size(); ++i) {
                        TypeInfo param_type = resolveType(method.params[i].second);
                        if (!param_type.conformsTo(base_method->parameter_types[i])) {
                            match = false;
                            break;
                        }
                    }
                }
                if (!match) {
                    report(ErrorType::REDEFINED_METHOD,
                           "El método '" + method.name.lexeme + "' ya existe en el tipo base '" + base + "' con una firma diferente",
                           method.name.line, method.name.col, "declaración de método");
                }
            }
        }
    }

    const std::string& class_name = stmt->name.lexeme;
    std::string old_class = current_class_;
    current_class_ = class_name;

    if (stmt->parent_name) {
        const std::string& parent_name = stmt->parent_name->lexeme;
        auto* base_class_decl = symbol_table_.getTypeDeclaration(parent_name);
        if (base_class_decl) {
            size_t base_param_count = base_class_decl->params.size();
            size_t base_args_count = stmt->parent_args.size();

            if (base_args_count > 0) {
                if (base_args_count != base_param_count) {
                    report(ErrorType::ARGUMENT_COUNT_MISMATCH,
                           "El constructor del padre '" + parent_name + "' espera " +
                               std::to_string(base_param_count) + " parámetro(s) pero se recibieron " +
                               std::to_string(base_args_count),
                           stmt->parent_name->line, stmt->parent_name->col, "declaración de clase");
                } else {
                    symbol_table_.enterScope();
                    for (const auto& param : stmt->params) {
                        symbol_table_.declareVariable(param.first.lexeme, resolveType(param.second));
                    }
                    for (size_t i = 0; i < base_args_count; ++i) {
                        visitExpr(stmt->parent_args[i].get());
                        TypeInfo arg_type = current_type_;
                        TypeInfo expected_param_type = resolveType(base_class_decl->params[i].second);
                        if (!arg_type.conformsTo(expected_param_type)) {
                            report(ErrorType::TYPE_ERROR,
                                   "El argumento " + std::to_string(i + 1) + " del constructor base tiene tipo '" + arg_type.toString() +
                                       "' pero se esperaba '" + expected_param_type.toString() + "'",
                                   stmt->parent_name->line, stmt->parent_name->col, "declaración de clase");
                        }
                    }
                    symbol_table_.exitScope();
                }
            } else if (base_param_count > 0) {
                if (stmt->params.size() != base_param_count) {
                    report(ErrorType::ARGUMENT_COUNT_MISMATCH,
                           "La clase '" + stmt->name.lexeme + "' debe declarar el mismo número de parámetros que su base '" +
                               parent_name + "' (" + std::to_string(base_param_count) + ") cuando no se pasan argumentos de base",
                           stmt->name.line, stmt->name.col, "declaración de clase");
                }
            }
        }
    }

    // --- E2: Attribute initialization type check ---
    symbol_table_.enterScope();
    
    // Declare 'self'
    symbol_table_.declareVariable("self", TypeInfo(TypeInfo::Kind::Object, class_name), false);

    // Declare class constructor parameters
    for (const auto& param : stmt->params) {
        symbol_table_.declareVariable(param.first.lexeme, resolveType(param.second));
    }

    // Check each attribute initializer
    for (const auto& attr : stmt->attributes) {
        if (attr.value) {
            visitExpr(attr.value.get());
            TypeInfo init_type = current_type_;
            TypeInfo declared_type = resolveType(attr.declared_type);

            if (attr.declared_type.has_value() && !init_type.conformsTo(declared_type)) {
                report(ErrorType::TYPE_ERROR,
                       "El tipo de la expresión de inicialización '" + init_type.toString() +
                           "' no conforma al tipo declarado del atributo '" + attr.name.lexeme +
                           "': '" + declared_type.toString() + "'",
                       attr.name.line, attr.name.col, "declaración de atributo");
            } else {
                if (!attr.declared_type.has_value()) {
                    auto type_sym = symbol_table_.lookupType(class_name);
                    if (type_sym) {
                        type_sym->attributes[attr.name.lexeme].type = init_type;
                    }
                }
            }
        }
    }

    symbol_table_.exitScope();

    // Analyze method bodies
    for (const auto& method : stmt->methods) {
        symbol_table_.enterScope();
        
        // Declare self
        symbol_table_.declareVariable("self", TypeInfo(TypeInfo::Kind::Object, class_name), false);
        
        // Declare method parameters
        for (const auto& param : method.params) {
            validateTypeExists(param.second);
            TypeInfo p_type = resolveType(param.second);
            symbol_table_.declareVariable(param.first.lexeme, p_type);
        }
        
        // Analyze method body
        visitExpr(method.body.get());
        TypeInfo body_type = current_type_;
        
        symbol_table_.exitScope();
        
        // Update method return type in symbol table if it was previously Unknown
        auto type_sym = symbol_table_.lookupType(class_name);
        if (type_sym) {
            auto method_sym = type_sym->methods[method.name.lexeme];
            if (method_sym && method_sym->return_type.getKind() == TypeInfo::Kind::Unknown) {
                method_sym->return_type = body_type;
            }
        }
        
        // Validate override conformance
        if (base != "Object") {
            auto base_method = symbol_table_.lookupMethod(base, method.name.lexeme);
            if (base_method) {
                if (!body_type.conformsTo(base_method->return_type)) {
                    report(ErrorType::TYPE_ERROR,
                           "El tipo de retorno del método '" + method.name.lexeme + "' ('" + body_type.toString() +
                               "') no conforma al tipo de retorno del método base '" + base + "': '" + base_method->return_type.toString() + "'",
                           method.name.line, method.name.col, "declaración de método");
                }
            }
        }
    }

    current_class_ = old_class;
}

void Phase2Analyzer::visit(parser::MethodDecl* stmt) {
    checkUniqueParamNames(stmt->params, stmt->name.line, stmt->name.col, "declaración de método");
    (void)stmt;
}

void Phase2Analyzer::visit(parser::AttributeDecl* stmt) { (void)stmt; }

void Phase2Analyzer::visit(parser::NumberExpr* expr) {
    (void)expr;
    current_type_ = TypeInfo(TypeInfo::Kind::Number);
}

void Phase2Analyzer::visit(parser::StringExpr* expr) {
    (void)expr;
    current_type_ = TypeInfo(TypeInfo::Kind::String);
}

void Phase2Analyzer::visit(parser::NullExpr* expr) {
    (void)expr;
    current_type_ = TypeInfo(TypeInfo::Kind::Null);
}

void Phase2Analyzer::visit(parser::BoolExpr* expr) {
    (void)expr;
    current_type_ = TypeInfo(TypeInfo::Kind::Boolean);
}

void Phase2Analyzer::visit(parser::SelfExpr* expr) {
    if (current_class_.empty()) {
        report(ErrorType::INVALID_SELF, "No se puede usar 'self' fuera del cuerpo de una clase", expr->token.line, expr->token.col, "expresión self");
        current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
    } else {
        current_type_ = TypeInfo(TypeInfo::Kind::Object, current_class_);
    }
}

void Phase2Analyzer::visit(parser::GroupedExpr* expr) {
    visitExpr(expr->expression.get());
}

void Phase2Analyzer::visit(parser::UnaryExpr* expr) {
    visitExpr(expr->right.get());
    TypeInfo operand_type = current_type_;
    const std::string& op = expr->op.lexeme;
    if (op == "-") {
        if (!operand_type.isNumeric() && !operand_type.isUnknown()) {
            report(ErrorType::TYPE_ERROR, "Operación unaria '-' inválida para el tipo '" + operand_type.toString() + "'", expr->op.line, expr->op.col, "expresión unaria");
        }
    } else if (op == "!" || op == "not") {
        if (!operand_type.isBoolean() && !operand_type.isUnknown()) {
            report(ErrorType::TYPE_ERROR, "Operación unaria '!' inválida para el tipo '" + operand_type.toString() + "'", expr->op.line, expr->op.col, "expresión unaria");
        }
    }
    current_type_ = TypeInfo::inferUnaryOp(op, operand_type);
}

void Phase2Analyzer::visit(parser::BinaryExpr* expr) {
    visitExpr(expr->left.get());
    TypeInfo left_type = current_type_;
    visitExpr(expr->right.get());
    TypeInfo right_type = current_type_;

    const std::string& op = expr->op.lexeme;
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "^") {
        if (!(left_type.isNumeric() || left_type.isUnknown()) || !(right_type.isNumeric() || right_type.isUnknown())) {
            report(ErrorType::TYPE_ERROR, "Operación aritmética inválida entre '" + left_type.toString() + "' y '" + right_type.toString() + "'", expr->op.line, expr->op.col, "expresión binaria");
        }
    } else if (op == "&&" || op == "||" || op == "and" || op == "or" || op == "&" || op == "|") {
        if (!(left_type.isBoolean() || left_type.isUnknown()) || !(right_type.isBoolean() || right_type.isUnknown())) {
            report(ErrorType::TYPE_ERROR, "Operación lógica inválida entre '" + left_type.toString() + "' y '" + right_type.toString() + "'", expr->op.line, expr->op.col, "expresión binaria");
        }
    } else if (op == "<" || op == "<=" || op == ">" || op == ">=") {
        if (!(left_type.isNumeric() || left_type.isUnknown()) || !(right_type.isNumeric() || right_type.isUnknown())) {
            report(ErrorType::TYPE_ERROR, "Comparación inválida entre '" + left_type.toString() + "' y '" + right_type.toString() + "'", expr->op.line, expr->op.col, "expresión binaria");
        }
    } else if (op == "==" || op == "!=") {
        if (left_type.getKind() != TypeInfo::Kind::Unknown && right_type.getKind() != TypeInfo::Kind::Unknown) {
            if (left_type.getKind() != right_type.getKind()) {
                report(ErrorType::TYPE_ERROR, "Comparación de igualdad inválida entre '" + left_type.toString() + "' y '" + right_type.toString() + "'", expr->op.line, expr->op.col, "expresión binaria");
            }
        }
    } else if (op == "@" || op == "@@") {
        if (left_type.isObject() || right_type.isObject()) {
            report(ErrorType::TYPE_ERROR, "Concatenación inválida entre '" + left_type.toString() + "' y '" + right_type.toString() + "'", expr->op.line, expr->op.col, "expresión binaria");
        }
    }

    current_type_ = TypeInfo::inferBinaryOp(op, left_type, right_type);
}

void Phase2Analyzer::visit(parser::CallExpr* expr) {
    TypeInfo ret_type = TypeInfo(TypeInfo::Kind::Unknown);
    std::vector<TypeInfo> arg_types;
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
        arg_types.push_back(current_type_);
    }

    if (auto* callee_id = dynamic_cast<parser::IdentifierExpr*>(expr->callee.get())) {
        resolveFunctionCall(callee_id->token, expr->args.size(), "llamada a función");
        auto func = symbol_table_.lookupFunction(callee_id->token.lexeme, expr->args.size());
        if (func) {
            ret_type = func->return_type;
            // Validate arguments conformance
            for (size_t i = 0; i < expr->args.size(); ++i) {
                if (i < func->parameter_types.size()) {
                    if (!arg_types[i].conformsTo(func->parameter_types[i])) {
                        report(ErrorType::TYPE_ERROR,
                               "El argumento " + std::to_string(i + 1) + " de la función '" + callee_id->token.lexeme +
                                   "' tiene tipo '" + arg_types[i].toString() + "' pero se esperaba '" +
                                   func->parameter_types[i].toString() + "'",
                               callee_id->token.line, callee_id->token.col, "llamada a función");
                    }
                }
            }
        }
    } else if (auto* get_attr = dynamic_cast<parser::GetAttrExpr*>(expr->callee.get())) {
        visitExpr(get_attr->object.get());
        TypeInfo obj_type = current_type_;
        if (!obj_type.isObject() && !obj_type.isUnknown()) {
            report(ErrorType::TYPE_ERROR, "No se puede llamar a un método en un tipo no objeto '" + obj_type.toString() + "'", get_attr->name.line, get_attr->name.col, "llamada a método");
            ret_type = TypeInfo(TypeInfo::Kind::Unknown);
        } else {
            auto method = symbol_table_.lookupMethod(obj_type.getTypeName(), get_attr->name.lexeme);
            if (!method && !obj_type.isUnknown()) {
                report(ErrorType::UNDEFINED_METHOD, "El tipo '" + obj_type.getTypeName() + "' no tiene un método '" + get_attr->name.lexeme + "'", get_attr->name.line, get_attr->name.col, "llamada a método");
                ret_type = TypeInfo(TypeInfo::Kind::Unknown);
            } else if (method) {
                if (expr->args.size() != method->parameter_types.size()) {
                    report(ErrorType::ARGUMENT_COUNT_MISMATCH, "El método '" + get_attr->name.lexeme + "' espera " + std::to_string(method->parameter_types.size()) + " argumento(s) pero se recibieron " + std::to_string(expr->args.size()), get_attr->name.line, get_attr->name.col, "llamada a método");
                } else {
                    for (size_t i = 0; i < expr->args.size(); ++i) {
                        if (!arg_types[i].conformsTo(method->parameter_types[i])) {
                            report(ErrorType::TYPE_ERROR,
                                   "El argumento " + std::to_string(i + 1) + " del método '" + get_attr->name.lexeme +
                                       "' tiene tipo '" + arg_types[i].toString() + "' pero se esperaba '" +
                                       method->parameter_types[i].toString() + "'",
                                   get_attr->name.line, get_attr->name.col, "llamada a método");
                        }
                    }
                }
                ret_type = method->return_type;
            } else {
                ret_type = TypeInfo(TypeInfo::Kind::Unknown);
            }
        }
    } else {
        visitExpr(expr->callee.get());
    }

    current_type_ = ret_type;
}

void Phase2Analyzer::visit(parser::GetAttrExpr* expr) {
    visitExpr(expr->object.get());
    TypeInfo obj_type = current_type_;
    if (!obj_type.isObject() && !obj_type.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "No se puede acceder a un atributo en un tipo no objeto '" + obj_type.toString() + "'", expr->dot.line, expr->dot.col, "acceso a atributo");
        current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
    } else {
        auto* attr = symbol_table_.lookupAttribute(obj_type.getTypeName(), expr->name.lexeme);
        if (!attr && !obj_type.isUnknown()) {
            report(ErrorType::UNDEFINED_ATTRIBUTE, "El tipo '" + obj_type.getTypeName() + "' no tiene un atributo '" + expr->name.lexeme + "'", expr->name.line, expr->name.col, "acceso a atributo");
            current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
        } else if (attr) {
            current_type_ = attr->type;
        } else {
            current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
        }
    }
}

void Phase2Analyzer::visit(parser::IfExpr* expr) {
    visitExpr(expr->condition.get());
    if (!current_type_.isBoolean() && !current_type_.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "La condición de 'if' debe ser de tipo Boolean, se obtuvo '" + current_type_.toString() + "'", 0, 0, "expresión if");
    }
    visitExpr(expr->then_branch.get());
    TypeInfo then_type = current_type_;
    visitExpr(expr->else_branch.get());
    TypeInfo else_type = current_type_;
    current_type_ = TypeInfo::getLowestCommonAncestor({then_type, else_type});
}

void Phase2Analyzer::visit(parser::WhileExpr* expr) {
    visitExpr(expr->condition.get());
    if (!current_type_.isBoolean() && !current_type_.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "La condición de 'while' debe ser de tipo Boolean, se obtuvo '" + current_type_.toString() + "'", 0, 0, "expresión while");
    }
    visitExpr(expr->body.get());
    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    }
    current_type_ = TypeInfo(TypeInfo::Kind::Void);
}

void Phase2Analyzer::visit(parser::ForExpr* expr) {
    visitExpr(expr->iterable.get());
    TypeInfo item_type = TypeInfo(TypeInfo::Kind::Unknown);
    if (auto* call = dynamic_cast<parser::CallExpr*>(expr->iterable.get())) {
        if (auto* id = dynamic_cast<parser::IdentifierExpr*>(call->callee.get())) {
            if (id->token.lexeme == "range") {
                item_type = TypeInfo(TypeInfo::Kind::Number);
            }
        }
    }
    symbol_table_.enterScope();
    symbol_table_.declareVariable(expr->variable.lexeme, item_type);
    visitExpr(expr->body.get());
    symbol_table_.exitScope();
    current_type_ = TypeInfo(TypeInfo::Kind::Void);
}

void Phase2Analyzer::visit(parser::WithExpr* expr) {
    visitExpr(expr->value.get());
    TypeInfo val_type = current_type_;
    TypeInfo body_type = TypeInfo(TypeInfo::Kind::Void);

    if (val_type.getKind() != TypeInfo::Kind::Null) {
        symbol_table_.enterScope();
        symbol_table_.declareVariable(expr->alias.lexeme, val_type);
        visitExpr(expr->body.get());
        body_type = current_type_;
        symbol_table_.exitScope();
    }

    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
        current_type_ = TypeInfo::getLowestCommonAncestor({body_type, current_type_});
    } else {
        current_type_ = body_type;
    }
}

void Phase2Analyzer::visit(parser::CaseExpr* expr) {
    visitExpr(expr->value.get());
    std::vector<TypeInfo> branch_types;
    for (const auto& branch : expr->branches) {
        symbol_table_.enterScope();
        symbol_table_.declareVariable(branch.name.lexeme, TypeInfo(TypeInfo::Kind::Object, branch.type_name.lexeme));
        visitExpr(branch.body.get());
        branch_types.push_back(current_type_);
        symbol_table_.exitScope();
    }
    current_type_ = TypeInfo::getLowestCommonAncestor(branch_types);
}

void Phase2Analyzer::visit(parser::IsExpr* expr) {
    visitExpr(expr->object.get());
    validateTypeExists(expr->type_name);
    current_type_ = TypeInfo(TypeInfo::Kind::Boolean);
}

void Phase2Analyzer::visit(parser::AsExpr* expr) {
    visitExpr(expr->object.get());
    TypeInfo obj_type = current_type_;
    if (!validateTypeExists(expr->type_name)) {
        current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
    } else {
        TypeInfo target_type = resolveType(expr->type_name);
        if (obj_type.getKind() != TypeInfo::Kind::Object &&
            obj_type.getKind() != TypeInfo::Kind::Unknown &&
            obj_type.getKind() != TypeInfo::Kind::Null) {
            
            bool match = false;
            if (obj_type.getKind() == TypeInfo::Kind::Number && target_type.getKind() == TypeInfo::Kind::Number) match = true;
            if (obj_type.getKind() == TypeInfo::Kind::String && target_type.getKind() == TypeInfo::Kind::String) match = true;
            if (obj_type.getKind() == TypeInfo::Kind::Boolean && target_type.getKind() == TypeInfo::Kind::Boolean) match = true;
            
            if (!match) {
                report(ErrorType::TYPE_ERROR, "Casteo inválido: no se puede convertir de '" + obj_type.toString() + "' a '" + target_type.toString() + "'", expr->type_name.line, expr->type_name.col, "expresión as");
            }
        }
        current_type_ = target_type;
    }
}

void Phase2Analyzer::visit(parser::AssignExpr* expr) {
    visitExpr(expr->rhs.get());
    TypeInfo rhs_type = current_type_;
    
    bool is_self = false;
    int line = 0, col = 0;
    if (auto* self_expr = dynamic_cast<parser::SelfExpr*>(expr->lhs.get())) {
        is_self = true;
        line = self_expr->token.line;
        col = self_expr->token.col;
    } else if (auto* id_expr = dynamic_cast<parser::IdentifierExpr*>(expr->lhs.get())) {
        if (id_expr->token.lexeme == "self") {
            is_self = true;
            line = id_expr->token.line;
            col = id_expr->token.col;
        }
    }
    
    if (is_self) {
        report(ErrorType::INVALID_SELF_ASSIGNMENT, "No se puede asignar a 'self' - no es un destino de asignación válido", line, col, "asignación destructiva");
    } else if (auto* lhs_id = dynamic_cast<parser::IdentifierExpr*>(expr->lhs.get())) {
        resolveVariableUse(lhs_id->token, "asignación");
        auto* sym = symbol_table_.lookupVariable(lhs_id->token.lexeme);
        if (sym) {
            if (!rhs_type.conformsTo(sym->type)) {
                report(ErrorType::TYPE_ERROR, "No se puede asignar un valor de tipo '" + rhs_type.toString() + "' a una variable de tipo '" + sym->type.toString() + "'", lhs_id->token.line, lhs_id->token.col, "asignación");
            }
        }
    } else {
        visitExpr(expr->lhs.get());
    }
    current_type_ = rhs_type;
}

void Phase2Analyzer::visit(parser::NewExpr* expr) {
    const std::string& type_name = expr->type_name.lexeme;
    std::vector<TypeInfo> arg_types;
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
        arg_types.push_back(current_type_);
    }

    if (!symbol_table_.isTypeDeclared(type_name)) {
        report(ErrorType::UNDEFINED_TYPE, "Tipo '" + type_name + "' no está definido",
               expr->type_name.line, expr->type_name.col, "instanciación de objeto");
        current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
    } else {
        auto* class_decl = symbol_table_.getTypeDeclaration(type_name);
        if (class_decl) {
            if (expr->args.size() != class_decl->params.size()) {
                report(ErrorType::ARGUMENT_COUNT_MISMATCH,
                       "El constructor de '" + type_name + "' espera " +
                           std::to_string(class_decl->params.size()) + " parámetro(s) pero se recibieron " +
                           std::to_string(expr->args.size()),
                       expr->type_name.line, expr->type_name.col, "instanciación de objeto");
            } else {
                for (size_t i = 0; i < expr->args.size(); ++i) {
                    TypeInfo expected_type = resolveType(class_decl->params[i].second);
                    if (!arg_types[i].conformsTo(expected_type)) {
                        report(ErrorType::TYPE_ERROR,
                               "El argumento " + std::to_string(i + 1) + " del constructor de '" + type_name +
                                   "' tiene tipo '" + arg_types[i].toString() + "' pero se esperaba '" +
                                   expected_type.toString() + "'",
                               expr->type_name.line, expr->type_name.col, "instanciación de objeto");
                    }
                }
            }
        }
        current_type_ = TypeInfo(TypeInfo::Kind::Object, type_name);
    }
}

void Phase2Analyzer::visit(parser::BaseCallExpr* expr) {
    bool has_base = false;
    if (!current_class_.empty()) {
        auto type_sym = symbol_table_.lookupType(current_class_);
        if (type_sym && type_sym->base_type != "Object" && !type_sym->base_type.empty()) {
            has_base = true;
        }
    }
    if (!has_base) {
        report(ErrorType::INVALID_BASE, "No se puede usar 'base' fuera de una definición de clase con herencia", 0, 0, "llamada base");
    }
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
    }
    current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
}

void Phase2Analyzer::visit(parser::SetAttrExpr* expr) {
    visitExpr(expr->object.get());
    TypeInfo obj_type = current_type_;
    visitExpr(expr->value.get());
    TypeInfo val_type = current_type_;
    if (!obj_type.isObject() && !obj_type.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "No se puede asignar un atributo en un tipo no objeto '" + obj_type.toString() + "'", expr->attr_name.line, expr->attr_name.col, "asignación de atributo");
    } else {
        auto* attr = symbol_table_.lookupAttribute(obj_type.getTypeName(), expr->attr_name.lexeme);
        if (!attr && !obj_type.isUnknown()) {
            report(ErrorType::UNDEFINED_ATTRIBUTE, "El tipo '" + obj_type.getTypeName() + "' no tiene un atributo '" + expr->attr_name.lexeme + "'", expr->attr_name.line, expr->attr_name.col, "asignación de atributo");
        } else if (attr) {
            if (!val_type.conformsTo(attr->type)) {
                report(ErrorType::TYPE_ERROR, "No se puede asignar un valor de tipo '" + val_type.toString() + "' al atributo '" + expr->attr_name.lexeme + "' de tipo '" + attr->type.toString() + "'", expr->attr_name.line, expr->attr_name.col, "asignación de atributo");
            }
        }
    }
}

void Phase2Analyzer::visit(parser::MethodCallExpr* expr) {
    visitExpr(expr->object.get());
    TypeInfo obj_type = current_type_;
    std::vector<TypeInfo> arg_types;
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
        arg_types.push_back(current_type_);
    }
    if (!obj_type.isObject() && !obj_type.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "No se puede llamar a un método en un tipo no objeto '" + obj_type.toString() + "'", expr->method_name.line, expr->method_name.col, "llamada a método");
        current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
    } else {
        auto method = symbol_table_.lookupMethod(obj_type.getTypeName(), expr->method_name.lexeme);
        if (!method && !obj_type.isUnknown()) {
            report(ErrorType::UNDEFINED_METHOD, "El tipo '" + obj_type.getTypeName() + "' no tiene un método '" + expr->method_name.lexeme + "'", expr->method_name.line, expr->method_name.col, "llamada a método");
            current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
        } else if (method) {
            if (expr->args.size() != method->parameter_types.size()) {
                report(ErrorType::ARGUMENT_COUNT_MISMATCH, "El método '" + expr->method_name.lexeme + "' espera " + std::to_string(method->parameter_types.size()) + " argumento(s) pero se recibieron " + std::to_string(expr->args.size()), expr->method_name.line, expr->method_name.col, "llamada a método");
            } else {
                for (size_t i = 0; i < expr->args.size(); ++i) {
                    if (!arg_types[i].conformsTo(method->parameter_types[i])) {
                        report(ErrorType::TYPE_ERROR,
                               "El argumento " + std::to_string(i + 1) + " del método '" + expr->method_name.lexeme +
                                   "' tiene tipo '" + arg_types[i].toString() + "' pero se esperaba '" +
                                   method->parameter_types[i].toString() + "'",
                               expr->method_name.line, expr->method_name.col, "llamada a método");
                    }
                }
            }
            current_type_ = method->return_type;
        } else {
            current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
        }
    }
}

void Phase2Analyzer::visit(parser::UnlessExpr* expr) {
    visitExpr(expr->condition.get());
    if (!current_type_.isBoolean() && !current_type_.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "La condición de 'unless' debe ser de tipo Boolean", 0, 0, "unless");
    }
    visitExpr(expr->then_branch.get());
    TypeInfo then_type = current_type_;
    visitExpr(expr->else_branch.get());
    TypeInfo else_type = current_type_;
    current_type_ = TypeInfo::getLowestCommonAncestor({then_type, else_type});
}

void Phase2Analyzer::visit(parser::RepeatExpr* expr) {
    visitExpr(expr->count.get());
    if (!current_type_.isNumeric() && !current_type_.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "La cantidad de repeticiones debe ser de tipo Number", 0, 0, "repeat");
    }
    visitExpr(expr->body.get());
    current_type_ = TypeInfo(TypeInfo::Kind::Void);
}

void Phase2Analyzer::visit(parser::LoopWhileExpr* expr) {
    visitExpr(expr->body.get());
    visitExpr(expr->condition.get());
    if (!current_type_.isBoolean() && !current_type_.isUnknown()) {
        report(ErrorType::TYPE_ERROR, "La condición de 'loop-while' debe ser de tipo Boolean", 0, 0, "loop-while");
    }
    current_type_ = TypeInfo(TypeInfo::Kind::Void);
}

void Phase2Analyzer::visit(parser::BlockExpr* expr) {
    TypeInfo block_type = TypeInfo(TypeInfo::Kind::Void);
    for (const auto& sub : expr->exprs) {
        visitExpr(sub.get());
        block_type = current_type_;
    }
    current_type_ = block_type;
}

void Phase2Analyzer::visit(parser::LetExpr* expr) {
    visitExpr(expr->initializer.get());
    TypeInfo init_type = current_type_;
    validateTypeExists(expr->declared_type);
    TypeInfo declared_type = resolveType(expr->declared_type);

    if (expr->declared_type.has_value() && !init_type.conformsTo(declared_type)) {
        report(ErrorType::TYPE_ERROR, "El tipo de la expresión no conforma al tipo declarado: esperado '" + declared_type.toString() + "', obtenido '" + init_type.toString() + "'", expr->name.line, expr->name.col, "expresión let");
    }

    symbol_table_.enterScope();

    if (symbol_table_.isBuiltinVariable(expr->name.lexeme)) {
        report(ErrorType::REDEFINED_VARIABLE,
               "Variable '" + expr->name.lexeme + "' ya está definida en este ámbito",
               expr->name.line, expr->name.col, "expresión let");
    } else {
        TypeInfo var_type = expr->declared_type.has_value() ? declared_type : init_type;
        if (!symbol_table_.declareVariable(expr->name.lexeme, var_type)) {
            report(ErrorType::REDEFINED_VARIABLE,
                   "Variable '" + expr->name.lexeme + "' ya está definida en este ámbito",
                   expr->name.line,
                   expr->name.col,
                   "expresión let");
        }
    }

    visitExpr(expr->body.get());
    symbol_table_.exitScope();
}

void Phase2Analyzer::visit(parser::IdentifierExpr* expr) {
    resolveVariableUse(expr->token, "identificador");
    auto* sym = symbol_table_.lookupVariable(expr->token.lexeme);
    if (sym) {
        current_type_ = sym->type;
    } else {
        current_type_ = TypeInfo(TypeInfo::Kind::Unknown);
    }
}

}  // namespace semantic
