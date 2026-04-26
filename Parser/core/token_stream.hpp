#pragma once

#include <cstddef>
#include <string>

#include "parse_error.hpp"
#include "token.hpp"

namespace parser {

class TokenStream {
public:
    explicit TokenStream(TokenList tokens);

    const Token& current() const;
    const Token& peek(std::size_t offset = 1) const;
    const Token& previous() const;

    const Token& advance();

    bool is(TokenType type) const;
    bool match(TokenType type);
    const Token& consume(TokenType type, const std::string& message);

    bool at_end() const;
    std::size_t position() const { return index_; }

private:
    TokenList tokens_;
    std::size_t index_ = 0;
};

}  // namespace parser
