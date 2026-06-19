#include "expr.hpp"

#include <sstream>
#include <utility>

namespace parser {

ExprStmt::ExprStmt(ExprPtr expr)
    : Stmt(StmtKind::EXPR), expr(std::move(expr)) {}

void ExprStmt::accept(StmtVisitor* visitor) {
    visitor->visit(this);
}

FunctionDecl::FunctionDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params, std::optional<Token> return_type, ExprPtr body)
    : Stmt(StmtKind::FUNCTION_DECL),
      name(std::move(name)),
      params(std::move(params)),
      return_type(std::move(return_type)),
      body(std::move(body)) {}

void FunctionDecl::accept(StmtVisitor* visitor) {
    visitor->visit(this);
}

ClassDecl::ClassDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params,
                     std::optional<Token> parent_name, std::vector<ExprPtr> parent_args,
                     std::vector<AttributeDef> attributes, std::vector<MethodDef> methods)
    : Stmt(StmtKind::CLASS_DECL),
      name(std::move(name)),
      params(std::move(params)),
      parent_name(std::move(parent_name)),
      parent_args(std::move(parent_args)),
      attributes(std::move(attributes)),
      methods(std::move(methods)) {}

void ClassDecl::accept(StmtVisitor* visitor) {
    visitor->visit(this);
}

AttributeDecl::AttributeDecl(Token name, ExprPtr initializer, std::optional<Token> declared_type)
    : Stmt(StmtKind::ATTRIBUTE_DECL),
      name(std::move(name)),
      initializer(std::move(initializer)),
      declared_type(std::move(declared_type)) {}

void AttributeDecl::accept(StmtVisitor* visitor) {
    visitor->visit(this);
}

MethodDecl::MethodDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params, std::optional<Token> return_type, ExprPtr body)
    : Stmt(StmtKind::METHOD_DECL),
      name(std::move(name)),
      params(std::move(params)),
      return_type(std::move(return_type)),
      body(std::move(body)) {}

void MethodDecl::accept(StmtVisitor* visitor) {
    visitor->visit(this);
}

Program::Program(std::vector<StmtPtr> stmts)
    : Stmt(StmtKind::PROGRAM), stmts(std::move(stmts)) {}

void Program::accept(StmtVisitor* visitor) {
    visitor->visit(this);
}

std::string stmt_to_string(const Stmt& stmt) {
    switch (stmt.stmt_kind) {
        case StmtKind::EXPR: {
            const auto& expr_stmt = static_cast<const ExprStmt&>(stmt);
            return "ExprStmt(" + expr_to_string(*expr_stmt.expr) + ")";
        }
        case StmtKind::FUNCTION_DECL: {
            const auto& func = static_cast<const FunctionDecl&>(stmt);
            std::ostringstream out;
            out << "FunctionDecl(" << func.name.lexeme << "(";
            for (std::size_t i = 0; i < func.params.size(); ++i) {
                if (i > 0) out << ", ";
                out << func.params[i].first.lexeme;
                if (func.params[i].second) {
                    out << ": " << func.params[i].second->lexeme;
                }
            }
            out << ")";
            if (func.return_type) {
                out << ": " << func.return_type->lexeme;
            }
            out << " => " << expr_to_string(*func.body) << ")";
            return out.str();
        }
        case StmtKind::CLASS_DECL: {
            const auto& class_decl = static_cast<const ClassDecl&>(stmt);
            std::ostringstream out;
            out << "ClassDecl(" << class_decl.name.lexeme;
            if (!class_decl.params.empty()) {
                out << "(";
                for (std::size_t i = 0; i < class_decl.params.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << class_decl.params[i].first.lexeme;
                    if (class_decl.params[i].second) {
                        out << ": " << class_decl.params[i].second->lexeme;
                    }
                }
                out << ")";
            }
            if (class_decl.parent_name) {
                out << " inherits " << class_decl.parent_name->lexeme;
                if (!class_decl.parent_args.empty()) {
                    out << "(";
                    for (std::size_t i = 0; i < class_decl.parent_args.size(); ++i) {
                        if (i > 0) out << ", ";
                        out << expr_to_string(*class_decl.parent_args[i]);
                    }
                    out << ")";
                }
            }
            out << " {\n";
            for (const auto& attr : class_decl.attributes) {
                out << "    " << attr.name.lexeme;
                if (attr.declared_type) {
                    out << ": " << attr.declared_type->lexeme;
                }
                out << " = " << expr_to_string(*attr.value) << ";\n";
            }
            for (const auto& method : class_decl.methods) {
                out << "    " << method.name.lexeme << "(";
                for (std::size_t i = 0; i < method.params.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << method.params[i].first.lexeme;
                    if (method.params[i].second) {
                        out << ": " << method.params[i].second->lexeme;
                    }
                }
                out << ") -> " << expr_to_string(*method.body) << ";\n";
            }
            out << "})";
            return out.str();
        }
        case StmtKind::PROGRAM: {
            const auto& prog = static_cast<const Program&>(stmt);
            return program_to_string(prog);
        }
        case StmtKind::METHOD_DECL: {
            const auto& method = static_cast<const MethodDecl&>(stmt);
            return "MethodDecl(" + method.name.lexeme + ")";
        }
        case StmtKind::ATTRIBUTE_DECL: {
            const auto& attr = static_cast<const AttributeDecl&>(stmt);
            return "AttributeDecl(" + attr.name.lexeme + ")";
        }
    }
    return "Stmt(?)";
}

std::string program_to_string(const Program& prog) {
    std::ostringstream out;
    out << "Program(\n";
    for (const auto& stmt : prog.stmts) {
        out << "  " << stmt_to_string(*stmt) << "\n";
    }
    out << ")";
    return out.str();
}

NumberExpr::NumberExpr(Token token)
    : Expr(ExprKind::NUMBER), token(std::move(token)) {}

void NumberExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

StringExpr::StringExpr(Token token)
    : Expr(ExprKind::STRING), token(std::move(token)) {}

void StringExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

NullExpr::NullExpr(Token token)
    : Expr(ExprKind::NULL_VALUE), token(std::move(token)) {}

void NullExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

BoolExpr::BoolExpr(Token token, bool value)
    : Expr(ExprKind::BOOL), token(std::move(token)), value(value) {}

void BoolExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

IdentifierExpr::IdentifierExpr(Token token)
    : Expr(ExprKind::IDENTIFIER), token(std::move(token)) {}

void IdentifierExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

SelfExpr::SelfExpr(Token token)
    : Expr(ExprKind::SELF_REF), token(std::move(token)) {}

void SelfExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

GroupedExpr::GroupedExpr(Token lparen, ExprPtr expression)
    : Expr(ExprKind::GROUPED),
      lparen(std::move(lparen)),
      expression(std::move(expression)) {}

void GroupedExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

UnaryExpr::UnaryExpr(Token op, ExprPtr right)
    : Expr(ExprKind::UNARY),
      op(std::move(op)),
      right(std::move(right)) {}

void UnaryExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

BinaryExpr::BinaryExpr(ExprPtr left, Token op, ExprPtr right)
    : Expr(ExprKind::BINARY),
      left(std::move(left)),
      op(std::move(op)),
      right(std::move(right)) {}

void BinaryExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

CallExpr::CallExpr(ExprPtr callee, Token lparen, std::vector<ExprPtr> args)
    : Expr(ExprKind::CALL),
      callee(std::move(callee)),
      lparen(std::move(lparen)),
      args(std::move(args)) {}

void CallExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

GetAttrExpr::GetAttrExpr(ExprPtr object, Token dot, Token name)
    : Expr(ExprKind::GET_ATTR),
      object(std::move(object)),
      dot(std::move(dot)),
      name(std::move(name)) {}

void GetAttrExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

LetExpr::LetExpr(Token name, std::optional<Token> declared_type, ExprPtr initializer, ExprPtr body)
    : Expr(ExprKind::LET),
      name(std::move(name)),
      declared_type(std::move(declared_type)),
      initializer(std::move(initializer)),
      body(std::move(body)) {}

void LetExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

BlockExpr::BlockExpr(std::vector<ExprPtr> exprs)
    : Expr(ExprKind::BLOCK),
      exprs(std::move(exprs)) {}

void BlockExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

IfExpr::IfExpr(ExprPtr condition, ExprPtr then_branch, ExprPtr else_branch)
    : Expr(ExprKind::IF),
      condition(std::move(condition)),
      then_branch(std::move(then_branch)),
      else_branch(std::move(else_branch)) {}

void IfExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

WhileExpr::WhileExpr(ExprPtr condition, ExprPtr body, ExprPtr else_branch)
    : Expr(ExprKind::WHILE),
      condition(std::move(condition)),
      body(std::move(body)),
      else_branch(std::move(else_branch)) {}

void WhileExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

ForExpr::ForExpr(Token variable, std::optional<Token> declared_type, ExprPtr iterable, ExprPtr body)
    : Expr(ExprKind::FOR),
      variable(std::move(variable)),
      declared_type(std::move(declared_type)),
      iterable(std::move(iterable)),
      body(std::move(body)) {}

void ForExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

WithExpr::WithExpr(ExprPtr value, Token alias, ExprPtr body, ExprPtr else_branch)
    : Expr(ExprKind::WITH),
      value(std::move(value)),
      alias(std::move(alias)),
      body(std::move(body)),
      else_branch(std::move(else_branch)) {}

void WithExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

CaseExpr::CaseExpr(ExprPtr value, std::vector<CaseBranchDef> branches)
    : Expr(ExprKind::CASE_EXPR),
      value(std::move(value)),
      branches(std::move(branches)) {}

void CaseExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

AsExpr::AsExpr(ExprPtr object, Token as_keyword, Token type_name)
    : Expr(ExprKind::AS_EXPR),
      object(std::move(object)),
      as_keyword(std::move(as_keyword)),
      type_name(std::move(type_name)) {}

void AsExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

IsExpr::IsExpr(ExprPtr object, Token is_keyword, Token type_name)
    : Expr(ExprKind::IS_EXPR),
      object(std::move(object)),
      is_keyword(std::move(is_keyword)),
      type_name(std::move(type_name)) {}

void IsExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

AssignExpr::AssignExpr(ExprPtr lhs, Token op, ExprPtr rhs)
    : Expr(ExprKind::ASSIGN_EXPR),
      lhs(std::move(lhs)),
      op(std::move(op)),
      rhs(std::move(rhs)) {}

void AssignExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

NewExpr::NewExpr(Token type_name, std::vector<ExprPtr> args)
    : Expr(ExprKind::NEW_OBJ),
      type_name(std::move(type_name)),
      args(std::move(args)) {}

void NewExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

BaseCallExpr::BaseCallExpr(std::vector<ExprPtr> args)
    : Expr(ExprKind::BASE_CALL),
      args(std::move(args)) {}

void BaseCallExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

SetAttrExpr::SetAttrExpr(ExprPtr object, Token attr_name, ExprPtr value)
    : Expr(ExprKind::ASSIGN_EXPR), // Map kind appropriately or extend enum if needed
      object(std::move(object)),
      attr_name(std::move(attr_name)),
      value(std::move(value)) {}

void SetAttrExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

MethodCallExpr::MethodCallExpr(ExprPtr object, Token method_name, std::vector<ExprPtr> args)
    : Expr(ExprKind::CALL),
      object(std::move(object)),
      method_name(std::move(method_name)),
      args(std::move(args)) {}

void MethodCallExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

UnlessExpr::UnlessExpr(ExprPtr condition, ExprPtr then_branch, ExprPtr else_branch)
    : Expr(ExprKind::UNLESS_EXPR),
      condition(std::move(condition)),
      then_branch(std::move(then_branch)),
      else_branch(std::move(else_branch)) {}

void UnlessExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

RepeatExpr::RepeatExpr(ExprPtr count, ExprPtr body)
    : Expr(ExprKind::REPEAT_EXPR),
      count(std::move(count)),
      body(std::move(body)) {}

void RepeatExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

LoopWhileExpr::LoopWhileExpr(ExprPtr body, ExprPtr condition)
    : Expr(ExprKind::LOOP_WHILE_EXPR),
      body(std::move(body)),
      condition(std::move(condition)) {}

void LoopWhileExpr::accept(ExprVisitor* visitor) {
    visitor->visit(this);
}

std::string expr_to_string(const Expr& expr) {
    switch (expr.kind) {
        case ExprKind::NUMBER: {
            const auto& number = static_cast<const NumberExpr&>(expr);
            return "Number(" + number.token.lexeme + ")";
        }
        case ExprKind::STRING: {
            const auto& string = static_cast<const StringExpr&>(expr);
            return "String(\"" + string.token.lexeme + "\")";
        }
        case ExprKind::NULL_VALUE: {
            return "Null";
        }
        case ExprKind::BOOL: {
            const auto& boolean = static_cast<const BoolExpr&>(expr);
            return boolean.value ? "Bool(true)" : "Bool(false)";
        }
        case ExprKind::IDENTIFIER: {
            const auto& identifier = static_cast<const IdentifierExpr&>(expr);
            return "Identifier(" + identifier.token.lexeme + ")";
        }
        case ExprKind::SELF_REF: {
            return "Self";
        }
        case ExprKind::GROUPED: {
            const auto& grouped = static_cast<const GroupedExpr&>(expr);
            return "Grouped(" + expr_to_string(*grouped.expression) + ")";
        }
        case ExprKind::UNARY: {
            const auto& unary = static_cast<const UnaryExpr&>(expr);
            return "Unary(" + unary.op.lexeme + ", " + expr_to_string(*unary.right) + ")";
        }
        case ExprKind::BINARY: {
            const auto& binary = static_cast<const BinaryExpr&>(expr);
            return "Binary(" + expr_to_string(*binary.left) + ", " + binary.op.lexeme +
                   ", " + expr_to_string(*binary.right) + ")";
        }
        case ExprKind::CALL: {
            const auto& call = static_cast<const CallExpr&>(expr);
            std::ostringstream out;
            out << "Call(" << expr_to_string(*call.callee) << ", [";
            for (std::size_t i = 0; i < call.args.size(); ++i) {
                if (i > 0) {
                    out << ", ";
                }
                out << expr_to_string(*call.args[i]);
            }
            out << "])";
            return out.str();
        }
        case ExprKind::GET_ATTR: {
            const auto& get_attr = static_cast<const GetAttrExpr&>(expr);
            return "GetAttr(" + expr_to_string(*get_attr.object) + ", " +
                   get_attr.name.lexeme + ")";
        }
        case ExprKind::LET: {
            const auto& let_expr = static_cast<const LetExpr&>(expr);
            std::ostringstream out;
            out << "Let(" << let_expr.name.lexeme;
            if (let_expr.declared_type) {
                out << ": " << let_expr.declared_type->lexeme;
            }
            out << " = " << expr_to_string(*let_expr.initializer)
                << " in " << expr_to_string(*let_expr.body) << ")";
            return out.str();
        }
        case ExprKind::BLOCK: {
            const auto& block = static_cast<const BlockExpr&>(expr);
            std::ostringstream out;
            out << "Block(";
            for (std::size_t i = 0; i < block.exprs.size(); ++i) {
                if (i > 0) out << ", ";
                out << expr_to_string(*block.exprs[i]);
            }
            out << ")";
            return out.str();
        }
        case ExprKind::IF: {
            const auto& if_expr = static_cast<const IfExpr&>(expr);
            std::ostringstream out;
            out << "If(" << expr_to_string(*if_expr.condition) << ", " << expr_to_string(*if_expr.then_branch);
            if (if_expr.else_branch) {
                out << ", " << expr_to_string(*if_expr.else_branch);
            }
            out << ")";
            return out.str();
        }
        case ExprKind::WHILE: {
            const auto& while_expr = static_cast<const WhileExpr&>(expr);
            std::ostringstream out;
            out << "While(" << expr_to_string(*while_expr.condition) << ", " << expr_to_string(*while_expr.body);
            if (while_expr.else_branch) {
                out << ", " << expr_to_string(*while_expr.else_branch);
            }
            out << ")";
            return out.str();
        }
        case ExprKind::FOR: {
            const auto& for_expr = static_cast<const ForExpr&>(expr);
            std::ostringstream out;
            out << "For(" << for_expr.variable.lexeme;
            if (for_expr.declared_type) {
                out << ": " << for_expr.declared_type->lexeme;
            }
            out << " in " << expr_to_string(*for_expr.iterable) << ", " << expr_to_string(*for_expr.body) << ")";
            return out.str();
        }
        case ExprKind::WITH: {
            const auto& with_expr = static_cast<const WithExpr&>(expr);
            std::ostringstream out;
            out << "With(" << expr_to_string(*with_expr.value)
                << " as " << with_expr.alias.lexeme
                << ", " << expr_to_string(*with_expr.body);
            if (with_expr.else_branch) {
                out << ", " << expr_to_string(*with_expr.else_branch);
            }
            out << ")";
            return out.str();
        }
        case ExprKind::CASE_EXPR: {
            const auto& case_expr = static_cast<const CaseExpr&>(expr);
            std::ostringstream out;
            out << "Case(" << expr_to_string(*case_expr.value) << ", [";
            for (std::size_t i = 0; i < case_expr.branches.size(); ++i) {
                if (i > 0) {
                    out << ", ";
                }
                out << case_expr.branches[i].name.lexeme
                    << ": " << case_expr.branches[i].type_name.lexeme
                    << " => " << expr_to_string(*case_expr.branches[i].body);
            }
            out << "])";
            return out.str();
        }
        case ExprKind::AS_EXPR: {
            const auto& as_expr = static_cast<const AsExpr&>(expr);
            return "As(" + expr_to_string(*as_expr.object) + ", " + as_expr.type_name.lexeme + ")";
        }
        case ExprKind::IS_EXPR: {
            const auto& is_expr = static_cast<const IsExpr&>(expr);
            return "Is(" + expr_to_string(*is_expr.object) + ", " + is_expr.type_name.lexeme + ")";
        }
        case ExprKind::ASSIGN_EXPR: {
            const auto& assign_expr = static_cast<const AssignExpr&>(expr);
            return "Assign(" + expr_to_string(*assign_expr.lhs) + ", " + assign_expr.op.lexeme + ", " +
                   expr_to_string(*assign_expr.rhs) + ")";
        }
        case ExprKind::NEW_OBJ: {
            const auto& new_expr = static_cast<const NewExpr&>(expr);
            std::ostringstream out;
            out << "New(" << new_expr.type_name.lexeme << "(";
            for (std::size_t i = 0; i < new_expr.args.size(); ++i) {
                if (i > 0) out << ", ";
                out << expr_to_string(*new_expr.args[i]);
            }
            out << "))";
            return out.str();
        }
        case ExprKind::BASE_CALL: {
            const auto& base_expr = static_cast<const BaseCallExpr&>(expr);
            std::ostringstream out;
            out << "BaseCall(";
            for (std::size_t i = 0; i < base_expr.args.size(); ++i) {
                if (i > 0) out << ", ";
                out << expr_to_string(*base_expr.args[i]);
            }
            out << ")";
            return out.str();
        }
        case ExprKind::UNLESS_EXPR: {
            const auto& unless_expr = static_cast<const UnlessExpr&>(expr);
            std::ostringstream out;
            out << "Unless(" << expr_to_string(*unless_expr.condition) << ", " << expr_to_string(*unless_expr.then_branch);
            if (unless_expr.else_branch) {
                out << ", " << expr_to_string(*unless_expr.else_branch);
            }
            out << ")";
            return out.str();
        }
        case ExprKind::REPEAT_EXPR: {
            const auto& repeat_expr = static_cast<const RepeatExpr&>(expr);
            return "Repeat(" + expr_to_string(*repeat_expr.count) + ", " + expr_to_string(*repeat_expr.body) + ")";
        }
        case ExprKind::LOOP_WHILE_EXPR: {
            const auto& loop_expr = static_cast<const LoopWhileExpr&>(expr);
            return "Loop(" + expr_to_string(*loop_expr.body) + ", " + expr_to_string(*loop_expr.condition) + ")";
        }
    }

    return "Expr(?)";
}

}  // namespace parser
