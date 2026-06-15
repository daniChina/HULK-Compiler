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

void Evaluator::visit(parser::Program* stmt) {
    for (const auto& s : stmt->stmts) {
        if (auto* fn = dynamic_cast<parser::FunctionDecl*>(s.get())) {
            registerFunction(fn);
            if (had_error_) {
                return;
            }
        }
    }

    for (const auto& s : stmt->stmts) {
        if (dynamic_cast<parser::FunctionDecl*>(s.get())) {
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

void Evaluator::visit(parser::ClassDecl* stmt) { (void)stmt; }
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
    current_ = value::Value(0.0);
}

void Evaluator::visit(parser::IdentifierExpr* expr) {
    if (auto* found = global_->lookup(expr->token.lexeme)) {
        current_ = *found;
        return;
    }
    setError("Variable '" + expr->token.lexeme + "' no está definida");
}

void Evaluator::visit(parser::SelfExpr* expr) { (void)expr; setError("self no implementado en Fase 3"); }

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
        current_ = value::Value(left.asString() + right.asString());
    } else if (op == "@@" && left.isString() && right.isString()) {
        current_ = value::Value(left.asString() + right.asString());
    } else if (left.isNumber() && right.isNumber() &&
               (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=")) {
        current_ = value::Value(compareNumbers(left.asNumber(), right.asNumber(), op));
    } else if (left.isBool() && right.isBool() &&
               (op == "==" || op == "!=")) {
        current_ = value::Value(op == "==" ? left.asBool() == right.asBool()
                                           : left.asBool() != right.asBool());
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
        if (args.size() != 1) {
            setError("print espera 1 argumento");
            return true;
        }
        visitExpr(args[0].get());
        if (!had_error_) {
            out_ << current_.toString() << '\n';
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

void Evaluator::visit(parser::GetAttrExpr* expr) { (void)expr; setError("atributo no implementado"); }
void Evaluator::visit(parser::WhileExpr* expr) { (void)expr; setError("while no implementado"); }
void Evaluator::visit(parser::ForExpr* expr) {
    visitExpr(expr->iterable.get());
    if (had_error_) {
        return;
    }

    std::shared_ptr<value::RangeIterator> iterator;
    if (current_.isRange()) {
        iterator = current_.asRange()->beginIterator();
    } else if (current_.isRangeIterator()) {
        iterator = current_.asRangeIterator();
    } else {
        setError("for requiere un iterable (p. ej. range)");
        return;
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
}
void Evaluator::visit(parser::WithExpr* expr) { (void)expr; setError("with no implementado"); }
void Evaluator::visit(parser::CaseExpr* expr) { (void)expr; setError("case no implementado"); }
void Evaluator::visit(parser::IsExpr* expr) { (void)expr; setError("is no implementado"); }
void Evaluator::visit(parser::AsExpr* expr) { (void)expr; setError("as no implementado"); }
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

    setError("Asignación no soportada para este destino");
}
void Evaluator::visit(parser::NewExpr* expr) { (void)expr; setError("new no implementado"); }
void Evaluator::visit(parser::BaseCallExpr* expr) { (void)expr; setError("base no implementado"); }
void Evaluator::visit(parser::SetAttrExpr* expr) { (void)expr; setError("set attr no implementado"); }
void Evaluator::visit(parser::MethodCallExpr* expr) { (void)expr; setError("método no implementado"); }
void Evaluator::visit(parser::UnlessExpr* expr) { (void)expr; setError("unless no implementado"); }
void Evaluator::visit(parser::RepeatExpr* expr) { (void)expr; setError("repeat no implementado"); }
void Evaluator::visit(parser::LoopWhileExpr* expr) { (void)expr; setError("loop no implementado"); }

}  // namespace eval
