#include "evaluator.hpp"

#include <cmath>
#include <cstdlib>

#include "../Value/iterable.hpp"

namespace eval {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

bool compareNumbers(double left, double right, const std::string& op) {
    if (op == "<") {
        return left < right;
    }
    if (op == "<=") {
        return left <= right;
    }
    if (op == ">") {
        return left > right;
    }
    if (op == ">=") {
        return left >= right;
    }
    if (op == "==") {
        return left == right;
    }
    if (op == "!=") {
        return left != right;
    }
    return false;
}

bool isLogicalAndOp(const std::string& op) {
    return op == "&&" || op == "and" || op == "&";
}

bool isLogicalOrOp(const std::string& op) {
    return op == "||" || op == "or" || op == "|";
}
}  // namespace

Evaluator::Evaluator(std::ostream& out) : out_(out), global_(std::make_shared<EnvFrame>()) {
    global_->define("PI", value::Value(kPi));
    global_->define("E", value::Value(kE));
}

bool Evaluator::isBuiltinFunctionName(const std::string& name) {
    return name == "print" || name == "sin" || name == "cos" || name == "sqrt" || name == "log" ||
           name == "exp" || name == "rand" || name == "range";
}

void Evaluator::setError(const std::string& message) {
    had_error_ = true;
    last_error_ = message;
}

void Evaluator::visitExpr(parser::Expr* expr) {
    if (expr && !had_error_) {
        expr->accept(this);
    }
}

void Evaluator::visitStmt(parser::Stmt* stmt) {
    if (stmt && !had_error_) {
        stmt->accept(this);
    }
}

value::Value Evaluator::evaluate(parser::Program* program) {
    had_error_ = false;
    last_error_.clear();
    functions_.clear();
    types_.clear();
    current_self_.reset();
    current_method_name_.clear();
    current_ = value::Value(0.0);
    if (!program) {
        return current_;
    }
    program->accept(this);
    return current_;
}

void Evaluator::registerFunction(parser::FunctionDecl* decl) {
    const std::string& name = decl->name.lexeme;
    if (isBuiltinFunctionName(name)) {
        setError("No se puede redefinir la función builtin '" + name + "'");
        return;
    }

    const std::size_t arity = decl->params.size();
    auto& overloads = functions_[name];
    if (overloads.find(arity) != overloads.end()) {
        setError("La función '" + name + "' ya está definida con aridad " + std::to_string(arity));
        return;
    }

    overloads[arity] = UserFunction{decl, global_};
}

void Evaluator::registerClass(parser::ClassDecl* decl) {
    const std::string& name = decl->name.lexeme;
    if (types_.find(name) != types_.end()) {
        setError("Tipo redefinido: " + name);
        return;
    }
    types_[name] = decl;
}

void Evaluator::initializeAttributes(value::Instance& instance, parser::ClassDecl* type_def) {
    auto saved = global_;
    global_ = instance.attrs;
    for (const auto& attr : type_def->attributes) {
        visitExpr(attr.value.get());
        if (had_error_) {
            global_ = saved;
            return;
        }
        instance.attrs->define(attr.name.lexeme, current_);
    }
    global_ = saved;
}

void Evaluator::initializeParentAttributes(value::Instance& instance, parser::ClassDecl* type_def) {
    if (!type_def->parent_name) {
        return;
    }

    const std::string& base_name = type_def->parent_name->lexeme;
    auto base_it = types_.find(base_name);
    if (base_it == types_.end()) {
        setError("Tipo base no registrado: " + base_name);
        return;
    }

    parser::ClassDecl* base = base_it->second;

    initializeParentAttributes(instance, base);
    if (had_error_) {
        return;
    }

    if (base->params.size() != type_def->parent_args.size()) {
        setError("Número incorrecto de argumentos en llamada a base: " + base_name);
        return;
    }

    auto base_env = std::make_shared<EnvFrame>(instance.attrs);
    auto saved = global_;
    global_ = base_env;
    for (std::size_t i = 0; i < base->params.size(); ++i) {
        visitExpr(type_def->parent_args[i].get());
        if (had_error_) {
            global_ = saved;
            return;
        }
        base_env->define(base->params[i].first.lexeme, current_);
    }

    for (const auto& attr : base->attributes) {
        visitExpr(attr.value.get());
        if (had_error_) {
            global_ = saved;
            return;
        }
        instance.attrs->define(attr.name.lexeme, current_);
    }
    global_ = saved;
}

value::Value Evaluator::constructInstance(parser::ClassDecl* type_def,
                                          const std::vector<parser::ExprPtr>& args) {
    if (type_def->params.size() != args.size()) {
        setError("Número de argumentos inválido para " + type_def->name.lexeme);
        return value::Value(0.0);
    }

    auto saved = global_;
    std::vector<value::Value> ctor_args;
    ctor_args.reserve(args.size());
    for (const auto& arg : args) {
        visitExpr(arg.get());
        if (had_error_) {
            return value::Value(0.0);
        }
        ctor_args.push_back(current_);
    }

    auto instance = std::make_shared<value::Instance>();
    instance->attrs = std::make_shared<EnvFrame>(saved);
    instance->type_def = type_def;
    instance->type_name = type_def->name.lexeme;
    instance->self = instance;

    for (std::size_t i = 0; i < type_def->params.size(); ++i) {
        instance->attrs->define(type_def->params[i].first.lexeme, ctor_args[i]);
    }

    initializeParentAttributes(*instance, type_def);
    if (had_error_) {
        global_ = saved;
        return value::Value(0.0);
    }

    initializeAttributes(*instance, type_def);
    global_ = saved;
    if (had_error_) {
        return value::Value(0.0);
    }

    return value::Value(instance);
}

std::shared_ptr<value::Instance> Evaluator::resolveInstance(parser::Expr* object_expr) {
    if (auto* self_expr = dynamic_cast<parser::SelfExpr*>(object_expr)) {
        (void)self_expr;
        if (!current_self_) {
            setError("Cannot use self outside of class methods");
            return nullptr;
        }
        return current_self_;
    }

    visitExpr(object_expr);
    if (had_error_) {
        return nullptr;
    }
    if (!current_.isInstance()) {
        setError("Cannot access attributes on non-class instance");
        return nullptr;
    }
    return current_.asInstance();
}

const parser::MethodDef* Evaluator::findMethod(parser::ClassDecl* type_def,
                                               const std::string& method_name) const {
    if (!type_def) {
        return nullptr;
    }
    for (const auto& method : type_def->methods) {
        if (method.name.lexeme == method_name) {
            return &method;
        }
    }
    if (type_def->parent_name) {
        auto it = types_.find(type_def->parent_name->lexeme);
        if (it != types_.end()) {
            return findMethod(it->second, method_name);
        }
    }
    return nullptr;
}

void Evaluator::invokeMethod(const std::shared_ptr<value::Instance>& instance,
                             const parser::MethodDef& method,
                             const std::vector<value::Value>& args) {
    const auto saved_global = global_;
    const auto saved_self = current_self_;
    const std::string saved_method = current_method_name_;

    global_ = std::make_shared<EnvFrame>(instance->attrs);
    current_self_ = instance;
    current_method_name_ = method.name.lexeme;
    global_->define("self", value::Value(instance));

    for (std::size_t i = 0; i < method.params.size(); ++i) {
        if (i < args.size()) {
            global_->define(method.params[i].first.lexeme, args[i]);
        }
    }

    visitExpr(method.body.get());

    global_ = saved_global;
    current_self_ = saved_self;
    current_method_name_ = saved_method;
}

bool Evaluator::assignInstanceAttr(const std::shared_ptr<value::Instance>& instance,
                                   const std::string& attr_name,
                                   const value::Value& value) {
    if (value::Value* slot = instance->attrs->lookup(attr_name)) {
        *slot = value;
        return true;
    }
    setError("Undefined attribute: " + attr_name);
    return false;
}

bool Evaluator::instanceConformsTo(parser::ClassDecl* dynamic_type,
                                   const std::string& static_type_name) const {
    if (!dynamic_type) {
        return false;
    }
    if (dynamic_type->name.lexeme == static_type_name) {
        return true;
    }
    if (dynamic_type->parent_name) {
        const std::string& parent = dynamic_type->parent_name->lexeme;
        if (parent == "Object") {
            return static_type_name == "Object";
        }
        auto it = types_.find(parent);
        if (it != types_.end()) {
            return instanceConformsTo(it->second, static_type_name);
        }
    }
    return false;
}

bool Evaluator::valueConformsTo(const value::Value& val, const std::string& target_type) const {
    if (!val.isInstance()) {
        if (target_type == "Null" && val.isNull()) {
            return true;
        }
        if (target_type == "Number" && val.isNumber()) {
            return true;
        }
        if (target_type == "String" && val.isString()) {
            return true;
        }
        if (target_type == "Boolean" && val.isBool()) {
            return true;
        }
        return false;
    }

    const auto instance = val.asInstance();
    if (!instance || !instance->type_def) {
        return false;
    }
    return instanceConformsTo(instance->type_def, target_type);
}

bool Evaluator::branchTypeIsMoreSpecific(const std::string& candidate,
                                         const std::string& current_best) const {
    const auto cand_it = types_.find(candidate);
    if (cand_it == types_.end()) {
        return false;
    }
    return instanceConformsTo(cand_it->second, current_best) && candidate != current_best;
}

void Evaluator::visit(parser::Program* stmt) {
    for (const auto& s : stmt->stmts) {
        if (auto* fn = dynamic_cast<parser::FunctionDecl*>(s.get())) {
            registerFunction(fn);
        } else if (auto* cls = dynamic_cast<parser::ClassDecl*>(s.get())) {
            registerClass(cls);
        }
        if (had_error_) {
            return;
        }
    }

    for (const auto& s : stmt->stmts) {
        if (dynamic_cast<parser::FunctionDecl*>(s.get()) ||
            dynamic_cast<parser::ClassDecl*>(s.get())) {
            continue;
        }
        visitStmt(s.get());
        if (had_error_) {
            break;
        }
    }
}

void Evaluator::visit(parser::ExprStmt* stmt) { visitExpr(stmt->expr.get()); }

void Evaluator::visit(parser::FunctionDecl* stmt) { (void)stmt; }

void Evaluator::visit(parser::ClassDecl* stmt) { registerClass(stmt); }
void Evaluator::visit(parser::MethodDecl* stmt) { (void)stmt; }
void Evaluator::visit(parser::AttributeDecl* stmt) { (void)stmt; }

void Evaluator::visit(parser::NumberExpr* expr) {
    current_ = value::Value(std::stod(expr->token.lexeme));
}

void Evaluator::visit(parser::StringExpr* expr) { current_ = value::Value(expr->token.lexeme); }

void Evaluator::visit(parser::BoolExpr* expr) {
    current_ = value::Value(expr->token.lexeme == "true");
}

void Evaluator::visit(parser::NullExpr* expr) {
    (void)expr;
    current_ = value::Value::null();
}

void Evaluator::visit(parser::IdentifierExpr* expr) {
    if (auto* found = global_->lookup(expr->token.lexeme)) {
        current_ = *found;
        return;
    }
    setError("Variable '" + expr->token.lexeme + "' no está definida");
}

void Evaluator::visit(parser::SelfExpr* expr) {
    (void)expr;
    if (!current_self_) {
        setError("Cannot use self outside of class methods");
        return;
    }
    current_ = value::Value(current_self_);
}

void Evaluator::visit(parser::GroupedExpr* expr) { visitExpr(expr->expression.get()); }

void Evaluator::visit(parser::UnaryExpr* expr) {
    visitExpr(expr->right.get());
    if (had_error_) {
        return;
    }
    if (expr->op.lexeme == "-") {
        current_ = value::Value(-current_.asNumber());
    } else if (expr->op.lexeme == "!") {
        current_ = value::Value(!current_.asBool());
    }
}

void Evaluator::visit(parser::BinaryExpr* expr) {
    const std::string& op = expr->op.lexeme;

    if (isLogicalAndOp(op)) {
        visitExpr(expr->left.get());
        if (had_error_) {
            return;
        }
        if (!current_.isBool()) {
            setError("Operador '" + op + "' requiere operandos booleanos");
            return;
        }
        if (!current_.asBool()) {
            current_ = value::Value(false);
            return;
        }
        visitExpr(expr->right.get());
        if (had_error_) {
            return;
        }
        if (!current_.isBool()) {
            setError("Operador '" + op + "' requiere operandos booleanos");
            return;
        }
        current_ = value::Value(current_.asBool());
        return;
    }

    if (isLogicalOrOp(op)) {
        visitExpr(expr->left.get());
        if (had_error_) {
            return;
        }
        if (!current_.isBool()) {
            setError("Operador '" + op + "' requiere operandos booleanos");
            return;
        }
        if (current_.asBool()) {
            current_ = value::Value(true);
            return;
        }
        visitExpr(expr->right.get());
        if (had_error_) {
            return;
        }
        if (!current_.isBool()) {
            setError("Operador '" + op + "' requiere operandos booleanos");
            return;
        }
        current_ = value::Value(current_.asBool());
        return;
    }

    visitExpr(expr->left.get());
    if (had_error_) {
        return;
    }
    const value::Value left = current_;
    visitExpr(expr->right.get());
    if (had_error_) {
        return;
    }
    const value::Value right = current_;

    if (op == "+" && left.isNumber() && right.isNumber()) {
        current_ = value::Value(left.asNumber() + right.asNumber());
    } else if (op == "-" && left.isNumber() && right.isNumber()) {
        current_ = value::Value(left.asNumber() - right.asNumber());
    } else if (op == "*" && left.isNumber() && right.isNumber()) {
        current_ = value::Value(left.asNumber() * right.asNumber());
    } else if (op == "/" && left.isNumber() && right.isNumber()) {
        current_ = value::Value(left.asNumber() / right.asNumber());
    } else if (op == "%" && left.isNumber() && right.isNumber()) {
        current_ = value::Value(std::fmod(left.asNumber(), right.asNumber()));
    } else if (op == "^" && left.isNumber() && right.isNumber()) {
        current_ = value::Value(std::pow(left.asNumber(), right.asNumber()));
    } else if (op == "@" && left.isString() && right.isString()) {
        current_ = value::Value(left.toString() + right.toString());
    } else if (op == "@@" && left.isString() && right.isString()) {
        current_ = value::Value(left.toString() + " " + right.toString());
    } else if (left.isNumber() && right.isNumber() &&
               (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=")) {
        current_ = value::Value(compareNumbers(left.asNumber(), right.asNumber(), op));
    } else if (left.isBool() && right.isBool() &&
               (op == "==" || op == "!=")) {
        current_ = value::Value(op == "==" ? left.asBool() == right.asBool()
                                           : left.asBool() != right.asBool());
    } else if (left.isString() && right.isString() &&
               (op == "==" || op == "!=")) {
        current_ = value::Value(op == "==" ? left.asString() == right.asString()
                                           : left.asString() != right.asString());
    } else {
        setError("Operador binario no soportado: " + op);
    }
}

void Evaluator::invokeUserFunction(const UserFunction& fn,
                                   const std::vector<parser::ExprPtr>& args) {
    if (!fn.decl) {
        setError("Función interna inválida");
        return;
    }
    if (args.size() != fn.decl->params.size()) {
        setError("La función '" + fn.decl->name.lexeme + "' espera " +
                 std::to_string(fn.decl->params.size()) + " argumento(s) pero se recibieron " +
                 std::to_string(args.size()));
        return;
    }

    auto frame = std::make_shared<EnvFrame>(fn.closure);
    for (std::size_t i = 0; i < fn.decl->params.size(); ++i) {
        visitExpr(args[i].get());
        if (had_error_) {
            return;
        }
        frame->define(fn.decl->params[i].first.lexeme, current_);
    }

    const auto saved = global_;
    global_ = frame;
    visitExpr(fn.decl->body.get());
    const value::Value result = current_;
    global_ = saved;
    current_ = result;
}

bool Evaluator::callBuiltin(const std::string& name, const std::vector<parser::ExprPtr>& args) {
    if (name == "print") {
        if (args.empty()) {
            out_ << '\n';
            current_ = value::Value(0.0);
            return true;
        }
        if (args.size() != 1) {
            setError("print espera 1 argumento");
            return true;
        }
        visitExpr(args[0].get());
        if (!had_error_) {
            out_ << current_ << '\n';
            current_ = value::Value(0.0);
        }
        return true;
    }

    if (name == "sin" || name == "cos" || name == "sqrt" || name == "log" || name == "exp") {
        if (args.size() != 1) {
            setError(name + " espera 1 argumento");
            return true;
        }
        visitExpr(args[0].get());
        if (had_error_) {
            return true;
        }
        const double x = current_.asNumber();
        if (name == "sin") {
            current_ = value::Value(std::sin(x));
        } else if (name == "cos") {
            current_ = value::Value(std::cos(x));
        } else if (name == "sqrt") {
            current_ = value::Value(std::sqrt(x));
        } else if (name == "log") {
            current_ = value::Value(std::log(x));
        } else {
            current_ = value::Value(std::exp(x));
        }
        return true;
    }

    if (name == "rand") {
        if (!args.empty()) {
            setError("rand no acepta argumentos");
            return true;
        }
        current_ = value::Value(static_cast<double>(std::rand()) / RAND_MAX);
        return true;
    }

    if (name == "range") {
        if (args.size() != 2) {
            setError("range espera 2 argumentos");
            return true;
        }
        visitExpr(args[0].get());
        if (had_error_) {
            return true;
        }
        const double start = current_.asNumber();
        visitExpr(args[1].get());
        if (had_error_) {
            return true;
        }
        const double end = current_.asNumber();
        current_ = value::Value(std::make_shared<value::RangeValue>(start, end));
        return true;
    }

    return false;
}

void Evaluator::visit(parser::CallExpr* expr) {
    if (auto* id = dynamic_cast<parser::IdentifierExpr*>(expr->callee.get())) {
        const std::string& name = id->token.lexeme;

        if (callBuiltin(name, expr->args)) {
            return;
        }

        const auto fn_it = functions_.find(name);
        if (fn_it != functions_.end()) {
            const auto overload_it = fn_it->second.find(expr->args.size());
            if (overload_it == fn_it->second.end()) {
                setError("La función '" + name + "' no está definida con aridad " +
                         std::to_string(expr->args.size()));
                return;
            }
            invokeUserFunction(overload_it->second, expr->args);
            return;
        }

        setError("Función '" + name + "' no está definida");
        return;
    }

    if (auto* get_attr = dynamic_cast<parser::GetAttrExpr*>(expr->callee.get())) {
        const auto instance = resolveInstance(get_attr->object.get());
        if (!instance || had_error_) {
            return;
        }

        const parser::MethodDef* method = findMethod(instance->type_def, get_attr->name.lexeme);
        if (!method) {
            setError("Undefined method: " + get_attr->name.lexeme);
            return;
        }
        if (method->params.size() != expr->args.size()) {
            setError("El método '" + get_attr->name.lexeme + "' espera " +
                     std::to_string(method->params.size()) + " argumento(s)");
            return;
        }

        std::vector<value::Value> args;
        args.reserve(expr->args.size());
        for (const auto& arg : expr->args) {
            visitExpr(arg.get());
            if (had_error_) {
                return;
            }
            args.push_back(current_);
        }

        invokeMethod(instance, *method, args);
        return;
    }

    setError("Llamada no implementada en Fase 3");
}

void Evaluator::visit(parser::LetExpr* expr) {
    visitExpr(expr->initializer.get());
    if (had_error_) {
        return;
    }
    const value::Value init = current_;
    auto frame = std::make_shared<EnvFrame>(global_);
    frame->define(expr->name.lexeme, init);
    auto saved = global_;
    global_ = frame;
    visitExpr(expr->body.get());
    global_ = saved;
}

void Evaluator::visit(parser::IfExpr* expr) {
    visitExpr(expr->condition.get());
    if (had_error_) {
        return;
    }
    if (current_.asBool()) {
        visitExpr(expr->then_branch.get());
    } else if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    }
}

void Evaluator::visit(parser::BlockExpr* expr) {
    for (const auto& sub : expr->exprs) {
        visitExpr(sub.get());
        if (had_error_) {
            break;
        }
    }
}

void Evaluator::visit(parser::GetAttrExpr* expr) {
    const auto instance = resolveInstance(expr->object.get());
    if (!instance || had_error_) {
        return;
    }
    if (value::Value* slot = instance->attrs->lookup(expr->name.lexeme)) {
        current_ = *slot;
        return;
    }
    setError("Undefined attribute: " + expr->name.lexeme);
}
void Evaluator::visit(parser::WhileExpr* expr) {
    value::Value result(0.0);
    bool ran_body = false;

    while (!had_error_) {
        visitExpr(expr->condition.get());
        if (had_error_) {
            return;
        }
        if (!current_.isBool()) {
            setError("La condición de 'while' debe ser booleana");
            return;
        }
        if (!current_.asBool()) {
            break;
        }

        visitExpr(expr->body.get());
        if (had_error_) {
            return;
        }
        result = current_;
        ran_body = true;
    }

    if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
        return;
    }

    if (ran_body) {
        current_ = result;
    } else {
        current_ = value::Value(0.0);
    }
}
void Evaluator::visit(parser::ForExpr* expr) {
    visitExpr(expr->iterable.get());
    if (had_error_) {
        return;
    }

    if (current_.isRange() || current_.isRangeIterator()) {
        std::shared_ptr<value::RangeIterator> iterator;
        if (current_.isRange()) {
            iterator = current_.asRange()->beginIterator();
        } else {
            iterator = current_.asRangeIterator();
        }

        while (iterator->next()) {
            auto frame = std::make_shared<EnvFrame>(global_);
            frame->define(expr->variable.lexeme, value::Value(iterator->currentValue()));
            const auto saved = global_;
            global_ = frame;
            visitExpr(expr->body.get());
            global_ = saved;
            if (had_error_) {
                break;
            }
        }
        return;
    }

    if (current_.isInstance()) {
        const auto instance = current_.asInstance();
        const parser::MethodDef* next_method = findMethod(instance->type_def, "next");
        const parser::MethodDef* current_method = findMethod(instance->type_def, "current");
        if (!next_method || !current_method) {
            setError("for requiere un iterable (objeto con next() y current())");
            return;
        }
        if (next_method->params.size() != 0 || current_method->params.size() != 0) {
            setError("next() y current() no deben recibir argumentos");
            return;
        }

        while (!had_error_) {
            invokeMethod(instance, *next_method, {});
            if (had_error_) {
                return;
            }
            if (!current_.isBool()) {
                setError("next() debe devolver Boolean");
                return;
            }
            if (!current_.asBool()) {
                break;
            }

            invokeMethod(instance, *current_method, {});
            if (had_error_) {
                return;
            }

            auto frame = std::make_shared<EnvFrame>(global_);
            frame->define(expr->variable.lexeme, current_);
            const auto saved = global_;
            global_ = frame;
            visitExpr(expr->body.get());
            global_ = saved;
            if (had_error_) {
                break;
            }
        }
        return;
    }

    setError("for requiere un iterable (p. ej. range o objeto con next() y current())");
}
void Evaluator::visit(parser::WithExpr* expr) {
    visitExpr(expr->value.get());
    if (had_error_) {
        return;
    }

    if (current_.isNull()) {
        if (expr->else_branch) {
            visitExpr(expr->else_branch.get());
        } else {
            current_ = value::Value::null();
        }
        return;
    }

    const value::Value bound = current_;
    auto frame = std::make_shared<EnvFrame>(global_);
    frame->define(expr->alias.lexeme, bound);
    const auto saved = global_;
    global_ = frame;
    visitExpr(expr->body.get());
    global_ = saved;
}
void Evaluator::visit(parser::CaseExpr* expr) {
    visitExpr(expr->value.get());
    if (had_error_) {
        return;
    }

    const value::Value scrutinee = current_;
    int best = -1;
    for (std::size_t i = 0; i < expr->branches.size(); ++i) {
        const auto& branch = expr->branches[i];
        if (!valueConformsTo(scrutinee, branch.type_name.lexeme)) {
            continue;
        }
        if (best < 0) {
            best = static_cast<int>(i);
        } else if (branchTypeIsMoreSpecific(branch.type_name.lexeme,
                                            expr->branches[static_cast<std::size_t>(best)].type_name.lexeme)) {
            best = static_cast<int>(i);
        }
    }

    if (best < 0) {
        setError("case: ninguna rama coincide con el valor");
        return;
    }

    const auto& branch = expr->branches[static_cast<std::size_t>(best)];
    auto frame = std::make_shared<EnvFrame>(global_);
    frame->define(branch.name.lexeme, scrutinee);
    const auto saved = global_;
    global_ = frame;
    visitExpr(branch.body.get());
    global_ = saved;
}
void Evaluator::visit(parser::IsExpr* expr) {
    visitExpr(expr->object.get());
    if (had_error_) {
        return;
    }
    const value::Value left = current_;
    const std::string& target = expr->type_name.lexeme;

    if (!left.isInstance()) {
        if (target == "Null" && left.isNull()) {
            current_ = value::Value(true);
        } else if (target == "Number" && left.isNumber()) {
            current_ = value::Value(true);
        } else if (target == "String" && left.isString()) {
            current_ = value::Value(true);
        } else if (target == "Boolean" && left.isBool()) {
            current_ = value::Value(true);
        } else {
            current_ = value::Value(false);
        }
        return;
    }

    const auto instance = left.asInstance();
    if (!instance || !instance->type_def) {
        current_ = value::Value(false);
        return;
    }

    current_ = value::Value(instanceConformsTo(instance->type_def, target));
}

void Evaluator::visit(parser::AsExpr* expr) {
    visitExpr(expr->object.get());
    if (had_error_) {
        return;
    }
    const value::Value left = current_;
    const std::string& target = expr->type_name.lexeme;

    if (!left.isInstance()) {
        if (target == "Null" && left.isNull()) {
            current_ = left;
        } else if (target == "Number" && left.isNumber()) {
            current_ = left;
        } else if (target == "String" && left.isString()) {
            current_ = left;
        } else if (target == "Boolean" && left.isBool()) {
            current_ = left;
        } else {
            setError("No se puede convertir de " + left.getTypeName() + " a " + target);
        }
        return;
    }

    const auto instance = left.asInstance();
    if (!instance || !instance->type_def) {
        setError("No se puede convertir instancia nula a " + target);
        return;
    }

    if (!instanceConformsTo(instance->type_def, target)) {
        setError("No se puede convertir de " + instance->type_def->name.lexeme + " a " + target);
        return;
    }

    current_ = left;
}
void Evaluator::visit(parser::AssignExpr* expr) {
    visitExpr(expr->rhs.get());
    if (had_error_) {
        return;
    }
    const value::Value rhs = current_;

    if (auto* id = dynamic_cast<parser::IdentifierExpr*>(expr->lhs.get())) {
        if (value::Value* slot = global_->lookup(id->token.lexeme)) {
            *slot = rhs;
            current_ = rhs;
            return;
        }
        setError("Variable '" + id->token.lexeme + "' no está definida");
        return;
    }

    if (auto* get_attr = dynamic_cast<parser::GetAttrExpr*>(expr->lhs.get())) {
        const auto instance = resolveInstance(get_attr->object.get());
        if (!instance || had_error_) {
            return;
        }
        if (assignInstanceAttr(instance, get_attr->name.lexeme, rhs)) {
            current_ = rhs;
        }
        return;
    }

    setError("Asignación no soportada para este destino");
}
void Evaluator::visit(parser::NewExpr* expr) {
    const std::string& type_name = expr->type_name.lexeme;
    auto it = types_.find(type_name);
    if (it == types_.end()) {
        setError("Tipo no registrado: " + type_name);
        return;
    }

    current_ = constructInstance(it->second, expr->args);
}
void Evaluator::visit(parser::BaseCallExpr* expr) {
    if (!current_self_) {
        setError("base() fuera de contexto de método");
        return;
    }
    if (current_method_name_.empty()) {
        setError("base() fuera de contexto de método");
        return;
    }

    parser::ClassDecl* type_def = current_self_->type_def;
    if (!type_def || !type_def->parent_name) {
        setError("base(): método base no encontrado");
        return;
    }

    const std::string& parent_name = type_def->parent_name->lexeme;
    if (parent_name == "Object") {
        setError("base(): método base no encontrado");
        return;
    }

    auto parent_it = types_.find(parent_name);
    if (parent_it == types_.end()) {
        setError("base(): método base no encontrado");
        return;
    }

    const parser::MethodDef* method = findMethod(parent_it->second, current_method_name_);
    if (!method) {
        setError("base(): método base no encontrado");
        return;
    }

    const auto saved_global = global_;
    const auto saved_self = current_self_;

    global_ = std::make_shared<EnvFrame>(current_self_->attrs);
    global_->define("self", value::Value(current_self_));

    for (const auto& param : method->params) {
        const std::string& param_name = param.first.lexeme;
        if (global_->bindings.find(param_name) != global_->bindings.end()) {
            continue;
        }
        if (value::Value* slot = saved_global->lookup(param_name)) {
            global_->define(param_name, *slot);
        }
    }

    visitExpr(method->body.get());

    global_ = saved_global;
    current_self_ = saved_self;
}
void Evaluator::visit(parser::SetAttrExpr* expr) {
    const auto instance = resolveInstance(expr->object.get());
    if (!instance || had_error_) {
        return;
    }

    visitExpr(expr->value.get());
    if (had_error_) {
        return;
    }

    if (assignInstanceAttr(instance, expr->attr_name.lexeme, current_)) {
        // current_ already holds assigned value
    }
}

void Evaluator::visit(parser::MethodCallExpr* expr) {
    const auto instance = resolveInstance(expr->object.get());
    if (!instance || had_error_) {
        return;
    }

    const parser::MethodDef* method = findMethod(instance->type_def, expr->method_name.lexeme);
    if (!method) {
        setError("Undefined method: " + expr->method_name.lexeme);
        return;
    }
    if (method->params.size() != expr->args.size()) {
        setError("El método '" + expr->method_name.lexeme + "' espera " +
                 std::to_string(method->params.size()) + " argumento(s)");
        return;
    }

    std::vector<value::Value> args;
    args.reserve(expr->args.size());
    for (const auto& arg : expr->args) {
        visitExpr(arg.get());
        if (had_error_) {
            return;
        }
        args.push_back(current_);
    }

    invokeMethod(instance, *method, args);
}
void Evaluator::visit(parser::UnlessExpr* expr) {
    visitExpr(expr->condition.get());
    if (had_error_) {
        return;
    }
    if (!current_.isBool()) {
        setError("La condición de 'unless' debe ser booleana");
        return;
    }

    if (!current_.asBool()) {
        visitExpr(expr->then_branch.get());
    } else if (expr->else_branch) {
        visitExpr(expr->else_branch.get());
    } else {
        current_ = value::Value(0.0);
    }
}

void Evaluator::visit(parser::RepeatExpr* expr) {
    visitExpr(expr->count.get());
    if (had_error_) {
        return;
    }
    if (!current_.isNumber()) {
        setError("La cantidad de 'repeat' debe ser un número");
        return;
    }

    const int times = static_cast<int>(current_.asNumber());
    if (times <= 0) {
        current_ = value::Value::null();
        return;
    }

    value::Value result(0.0);
    for (int i = 0; i < times && !had_error_; ++i) {
        visitExpr(expr->body.get());
        if (had_error_) {
            return;
        }
        result = current_;
    }
    current_ = result;
}

void Evaluator::visit(parser::LoopWhileExpr* expr) {
    visitExpr(expr->body.get());
    if (had_error_) {
        return;
    }
    value::Value result = current_;

    while (!had_error_) {
        visitExpr(expr->condition.get());
        if (had_error_) {
            return;
        }
        if (!current_.isBool()) {
            setError("La condición de 'loop-while' debe ser booleana");
            return;
        }
        if (!current_.asBool()) {
            break;
        }

        visitExpr(expr->body.get());
        if (had_error_) {
            return;
        }
        result = current_;
    }

    current_ = result;
}

}  // namespace eval
