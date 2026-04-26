#pragma once

#include "../ast/expr.hpp"
#include "../core/token_stream.hpp"

namespace parser {

class Parser {
public:
    explicit Parser(TokenList tokens);

    ExprPtr parse_expression();
    ExprPtr parse_expression_statement();

private:
    ExprPtr parse_or();
    ExprPtr parse_and();
    ExprPtr parse_comparison();
    ExprPtr parse_concat();
    ExprPtr parse_add();
    ExprPtr parse_mul();
    ExprPtr parse_power();
    ExprPtr parse_unary();
    ExprPtr parse_primary();

    TokenStream tokens_;
};

}  // namespace parser
