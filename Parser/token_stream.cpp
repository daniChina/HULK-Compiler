#include "token_stream.hpp"

#include <algorithm>
#include <utility>

namespace parser {

TokenStream::TokenStream(TokenList tokens)
    : tokens_(std::move(tokens)) {
    if (tokens_.empty()) {
        tokens_.push_back(Token{TokenType::EOF_TOKEN, "", 1, 1});
    }
}

const Token& TokenStream::current() const {
    return tokens_[std::min(index_, tokens_.size() - 1)];
}

const Token& TokenStream::peek(std::size_t offset) const {
    return tokens_[std::min(index_ + offset, tokens_.size() - 1)];
}

const Token& TokenStream::previous() const {
    if (index_ == 0) {
        return tokens_.front();
    }
    return tokens_[index_ - 1];
}

const Token& TokenStream::advance() {
    if (!at_end()) {
        ++index_;
    }
    return previous();
}

bool TokenStream::is(TokenType type) const {
    return current().type == type;
}

bool TokenStream::match(TokenType type) {
    if (!is(type)) {
        return false;
    }
    advance();
    return true;
}

const Token& TokenStream::consume(TokenType type, const std::string& message) {
    if (!is(type)) {
        throw ParseError(current(), message);
    }
    return advance();
}

bool TokenStream::at_end() const {
    return current().type == TokenType::EOF_TOKEN;
}

}  // namespace parser
