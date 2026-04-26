#include "parser.hpp"

#include <utility>

namespace parser {

Parser::Parser(TokenList tokens)
    : tokens_(std::move(tokens)) {}

ExprPtr Parser::parse_expression() {
    return parse_or();
}

ExprPtr Parser::parse_expression_statement() {
    auto expr = parse_expression();
    tokens_.consume(TokenType::SEMICOLON, "Se esperaba ';' al final de la expresion");
    return expr;
}

ExprPtr Parser::parse_primary() {
    if (tokens_.is(TokenType::NUMBER_LITERAL)) {
        return std::make_unique<NumberExpr>(tokens_.advance());
    }

    if (tokens_.is(TokenType::STRING_LITERAL)) {
        return std::make_unique<StringExpr>(tokens_.advance());
    }

    if (tokens_.is(TokenType::TRUE)) {
        return std::make_unique<BoolExpr>(tokens_.advance(), true);
    }

    if (tokens_.is(TokenType::FALSE)) {
        return std::make_unique<BoolExpr>(tokens_.advance(), false);
    }

    if (tokens_.is(TokenType::IDENTIFIER)) {
        return std::make_unique<IdentifierExpr>(tokens_.advance());
    }

    if (tokens_.match(TokenType::LPAREN)) {
        Token lparen = tokens_.previous();
        auto expression = parse_expression();
        tokens_.consume(TokenType::RPAREN, "Se esperaba ')' para cerrar la expresion agrupada");
        return std::make_unique<GroupedExpr>(std::move(lparen), std::move(expression));
    }

    throw ParseError(
        tokens_.current(),
        "Se esperaba una expresion primaria: numero, string, booleano, identificador o '('");
}

ExprPtr Parser::parse_or() {
    auto expr = parse_and();

    while (tokens_.is(TokenType::OR)) {
        Token op = tokens_.advance();
        auto right = parse_and();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_and() {
    auto expr = parse_comparison();

    while (tokens_.is(TokenType::AND)) {
        Token op = tokens_.advance();
        auto right = parse_comparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_comparison() {
    auto expr = parse_concat();

    while (tokens_.is(TokenType::LESS) ||
           tokens_.is(TokenType::LESS_EQUAL) ||
           tokens_.is(TokenType::GREATER) ||
           tokens_.is(TokenType::GREATER_EQUAL) ||
           tokens_.is(TokenType::EQUAL_EQUAL) ||
           tokens_.is(TokenType::BANG_EQUAL)) {
        Token op = tokens_.advance();
        auto right = parse_concat();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_concat() {
    auto expr = parse_add();

    while (tokens_.is(TokenType::CONCAT) || tokens_.is(TokenType::CONCAT_WS)) {
        Token op = tokens_.advance();
        auto right = parse_add();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_add() {
    auto expr = parse_mul();

    while (tokens_.is(TokenType::PLUS) || tokens_.is(TokenType::MINUS)) {
        Token op = tokens_.advance();
        auto right = parse_mul();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_mul() {
    auto expr = parse_power();

    while (tokens_.is(TokenType::STAR) || tokens_.is(TokenType::SLASH)) {
        Token op = tokens_.advance();
        auto right = parse_power();
        expr = std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_power() {
    auto expr = parse_unary();

    if (tokens_.is(TokenType::CARET)) {
        Token op = tokens_.advance();
        auto right = parse_power();
        return std::make_unique<BinaryExpr>(std::move(expr), std::move(op), std::move(right));
    }

    return expr;
}

ExprPtr Parser::parse_unary() {
    if (tokens_.is(TokenType::MINUS) ||
        tokens_.is(TokenType::BANG) ||
        tokens_.is(TokenType::TILDE)) {
        Token op = tokens_.advance();
        auto right = parse_unary();
        return std::make_unique<UnaryExpr>(std::move(op), std::move(right));
    }

    return parse_primary();
}

}  // namespace parser
