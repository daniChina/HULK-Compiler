#include "expr.hpp"

#include <sstream>
#include <utility>

namespace parser {

NumberExpr::NumberExpr(Token token)
    : Expr(ExprKind::NUMBER), token(std::move(token)) {}

StringExpr::StringExpr(Token token)
    : Expr(ExprKind::STRING), token(std::move(token)) {}

BoolExpr::BoolExpr(Token token, bool value)
    : Expr(ExprKind::BOOL), token(std::move(token)), value(value) {}

IdentifierExpr::IdentifierExpr(Token token)
    : Expr(ExprKind::IDENTIFIER), token(std::move(token)) {}

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
        case ExprKind::BOOL: {
            const auto& boolean = static_cast<const BoolExpr&>(expr);
            return boolean.value ? "Bool(true)" : "Bool(false)";
        }
        case ExprKind::IDENTIFIER: {
            const auto& identifier = static_cast<const IdentifierExpr&>(expr);
            return "Identifier(" + identifier.token.lexeme + ")";
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
    }

    return "Expr(?)";
}

}  // namespace parser
