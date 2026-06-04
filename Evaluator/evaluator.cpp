#include "evaluator.hpp"

#include <cmath>

namespace eval {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;
}  // namespace

Evaluator::Evaluator(std::ostream& out) : out_(out), global_(std::make_shared<EnvFrame>()) {
    global_->define("PI", value::Value(kPi));
    global_->define("E", value::Value(kE));
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
    current_ = value::Value(0.0);
    if (!program) {
        return current_;
    }
    program->accept(this);
    return current_;
}

void Evaluator::visit(parser::Program* stmt) {
    for (const auto& s : stmt->stmts) {
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
    const std::string& op = expr->op.lexeme;
    if (op == "+") {
        current_ = value::Value(left.asNumber() + right.asNumber());
    } else if (op == "-") {
        current_ = value::Value(left.asNumber() - right.asNumber());
    } else if (op == "*") {
        current_ = value::Value(left.asNumber() * right.asNumber());
    } else if (op == "/") {
        current_ = value::Value(left.asNumber() / right.asNumber());
    } else if (op == "@") {
        current_ = value::Value(left.asString() + right.asString());
    }
}

void Evaluator::visit(parser::CallExpr* expr) {
    if (auto* id = dynamic_cast<parser::IdentifierExpr*>(expr->callee.get())) {
        if (id->token.lexeme == "print" && expr->args.size() == 1) {
            visitExpr(expr->args[0].get());
            if (!had_error_) {
                out_ << current_.toString() << '\n';
                current_ = value::Value(0.0);
            }
            return;
        }
        if (id->token.lexeme == "sin" && expr->args.size() == 1) {
            visitExpr(expr->args[0].get());
            if (!had_error_) {
                current_ = value::Value(std::sin(current_.asNumber()));
            }
            return;
        }
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
void Evaluator::visit(parser::ForExpr* expr) { (void)expr; setError("for no implementado"); }
void Evaluator::visit(parser::WithExpr* expr) { (void)expr; setError("with no implementado"); }
void Evaluator::visit(parser::CaseExpr* expr) { (void)expr; setError("case no implementado"); }
void Evaluator::visit(parser::IsExpr* expr) { (void)expr; setError("is no implementado"); }
void Evaluator::visit(parser::AsExpr* expr) { (void)expr; setError("as no implementado"); }
void Evaluator::visit(parser::AssignExpr* expr) { (void)expr; setError("asignación no implementada"); }
void Evaluator::visit(parser::NewExpr* expr) { (void)expr; setError("new no implementado"); }
void Evaluator::visit(parser::BaseCallExpr* expr) { (void)expr; setError("base no implementado"); }
void Evaluator::visit(parser::SetAttrExpr* expr) { (void)expr; setError("set attr no implementado"); }
void Evaluator::visit(parser::MethodCallExpr* expr) { (void)expr; setError("método no implementado"); }
void Evaluator::visit(parser::UnlessExpr* expr) { (void)expr; setError("unless no implementado"); }
void Evaluator::visit(parser::RepeatExpr* expr) { (void)expr; setError("repeat no implementado"); }
void Evaluator::visit(parser::LoopWhileExpr* expr) { (void)expr; setError("loop no implementado"); }

}  // namespace eval
