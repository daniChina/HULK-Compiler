#include "expr.hpp"

#include <sstream>
#include <utility>

namespace parser {

ExprStmt::ExprStmt(ExprPtr expr)
    : Stmt(StmtKind::EXPR), expr(std::move(expr)) {}

FunctionDecl::FunctionDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params, std::optional<Token> return_type, ExprPtr body)
    : Stmt(StmtKind::FUNCTION_DECL),
      name(std::move(name)),
      params(std::move(params)),
      return_type(std::move(return_type)),
      body(std::move(body)) {}

TypeDecl::TypeDecl(Token name, std::vector<std::pair<Token, std::optional<Token>>> params,
             std::optional<Token> parent_name, std::vector<ExprPtr> parent_args,
             std::vector<AttributeDef> attributes, std::vector<MethodDef> methods)
    : Stmt(StmtKind::TYPE_DECL),
      name(std::move(name)),
      params(std::move(params)),
      parent_name(std::move(parent_name)),
      parent_args(std::move(parent_args)),
      attributes(std::move(attributes)),
      methods(std::move(methods)) {}

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
        case StmtKind::TYPE_DECL: {
            const auto& type_decl = static_cast<const TypeDecl&>(stmt);
            std::ostringstream out;
            out << "TypeDecl(" << type_decl.name.lexeme << "(";
            for (std::size_t i = 0; i < type_decl.params.size(); ++i) {
                if (i > 0) out << ", ";
                out << type_decl.params[i].first.lexeme;
                if (type_decl.params[i].second) {
                    out << ": " << type_decl.params[i].second->lexeme;
                }
            }
            out << ")";
            if (type_decl.parent_name) {
                out << " inherits " << type_decl.parent_name->lexeme << "(";
                for (std::size_t i = 0; i < type_decl.parent_args.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << expr_to_string(*type_decl.parent_args[i]);
                }
                out << ")";
            }
            out << " {\n";
            for (const auto& attr : type_decl.attributes) {
                out << "    " << attr.name.lexeme << " = " << expr_to_string(*attr.value) << ";\n";
            }
            for (const auto& method : type_decl.methods) {
                out << "    " << method.name.lexeme << "(";
                for (std::size_t i = 0; i < method.params.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << method.params[i].first.lexeme;
                    if (method.params[i].second) {
                        out << ": " << method.params[i].second->lexeme;
                    }
                }
                out << ")";
                if (method.return_type) {
                    out << ": " << method.return_type->lexeme;
                }
                out << " => " << expr_to_string(*method.body) << ";\n";
            }
            out << "})";
            return out.str();
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

StringExpr::StringExpr(Token token)
    : Expr(ExprKind::STRING), token(std::move(token)) {}

NullExpr::NullExpr(Token token)
    : Expr(ExprKind::NULL_VALUE), token(std::move(token)) {}

BoolExpr::BoolExpr(Token token, bool value)
    : Expr(ExprKind::BOOL), token(std::move(token)), value(value) {}

IdentifierExpr::IdentifierExpr(Token token)
    : Expr(ExprKind::IDENTIFIER), token(std::move(token)) {}

SelfExpr::SelfExpr(Token token)
    : Expr(ExprKind::SELF_REF), token(std::move(token)) {}

GroupedExpr::GroupedExpr(Token lparen, ExprPtr expression)
    : Expr(ExprKind::GROUPED),
      lparen(std::move(lparen)),
      expression(std::move(expression)) {}

UnaryExpr::UnaryExpr(Token op, ExprPtr right)
    : Expr(ExprKind::UNARY),
      op(std::move(op)),
      right(std::move(right)) {}

BinaryExpr::BinaryExpr(ExprPtr left, Token op, ExprPtr right)
    : Expr(ExprKind::BINARY),
      left(std::move(left)),
      op(std::move(op)),
      right(std::move(right)) {}

CallExpr::CallExpr(ExprPtr callee, Token lparen, std::vector<ExprPtr> args)
    : Expr(ExprKind::CALL),
      callee(std::move(callee)),
      lparen(std::move(lparen)),
      args(std::move(args)) {}

GetAttrExpr::GetAttrExpr(ExprPtr object, Token dot, Token name)
    : Expr(ExprKind::GET_ATTR),
      object(std::move(object)),
      dot(std::move(dot)),
      name(std::move(name)) {}

LetExpr::LetExpr(Token name, std::optional<Token> declared_type, ExprPtr initializer, ExprPtr body)
    : Expr(ExprKind::LET),
      name(std::move(name)),
      declared_type(std::move(declared_type)),
      initializer(std::move(initializer)),
      body(std::move(body)) {}

BlockExpr::BlockExpr(std::vector<ExprPtr> exprs)
    : Expr(ExprKind::BLOCK),
      exprs(std::move(exprs)) {}

IfExpr::IfExpr(ExprPtr condition, ExprPtr then_branch, ExprPtr else_branch)
    : Expr(ExprKind::IF),
      condition(std::move(condition)),
      then_branch(std::move(then_branch)),
      else_branch(std::move(else_branch)) {}

WhileExpr::WhileExpr(ExprPtr condition, ExprPtr body, ExprPtr else_branch)
    : Expr(ExprKind::WHILE),
      condition(std::move(condition)),
      body(std::move(body)),
      else_branch(std::move(else_branch)) {}

ForExpr::ForExpr(Token variable, ExprPtr iterable, ExprPtr body)
    : Expr(ExprKind::FOR),
      variable(std::move(variable)),
      iterable(std::move(iterable)),
      body(std::move(body)) {}

WithExpr::WithExpr(ExprPtr value, Token alias, ExprPtr body, ExprPtr else_branch)
    : Expr(ExprKind::WITH),
      value(std::move(value)),
      alias(std::move(alias)),
      body(std::move(body)),
      else_branch(std::move(else_branch)) {}

NewExpr::NewExpr(Token type_name, std::vector<ExprPtr> args)
    : Expr(ExprKind::NEW_OBJ),
      type_name(std::move(type_name)),
      args(std::move(args)) {}

BaseCallExpr::BaseCallExpr(std::vector<ExprPtr> args)
    : Expr(ExprKind::BASE_CALL),
      args(std::move(args)) {}

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
            // El AST conserva Null como literal canonico sin detalles extra.
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
            return "For(" + for_expr.variable.lexeme + " in " + expr_to_string(*for_expr.iterable) + ", " + expr_to_string(*for_expr.body) + ")";
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
    }

    return "Expr(?)";
}

}  // namespace parser
