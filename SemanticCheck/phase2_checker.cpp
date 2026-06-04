#include "phase2_checker.hpp"

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
    program->accept(this);
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
    (void)stmt;
    // R4: registro en orden textual — pendiente.
}

void Phase2Analyzer::visit(parser::ClassDecl* stmt) { (void)stmt; }
void Phase2Analyzer::visit(parser::MethodDecl* stmt) { (void)stmt; }
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
    visitExpr(expr->callee.get());
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
    visitExpr(expr->body.get());
}
void Phase2Analyzer::visit(parser::WithExpr* expr) {
    visitExpr(expr->value.get());
    visitExpr(expr->body.get());
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
    visitExpr(expr->lhs.get());
    visitExpr(expr->rhs.get());
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

    if (!symbol_table_.declareVariable(expr->name.lexeme, TypeInfo(TypeInfo::Kind::Unknown))) {
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
    (void)expr;
    // R2: lookupVariable — pendiente en este lote.
}

}  // namespace semantic
