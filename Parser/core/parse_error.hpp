#pragma once

#include <stdexcept>
#include <string>

#include "token.hpp"

namespace parser {

class ParseError : public std::runtime_error {
public:
    ParseError(const Token& found, const std::string& message)
        : std::runtime_error(build_message(found, message)),
          found_(found),
          user_message_(message) {}

    const Token& found() const { return found_; }
    const std::string& user_message() const { return user_message_; }

private:
    Token found_;
    std::string user_message_;

    static std::string build_message(const Token& found, const std::string& message) {
        return "Error de parseo en linea " + std::to_string(found.line) +
               ", columna " + std::to_string(found.col) +
               ": " + message +
               " (token encontrado: " + token_name(found.type) +
               ", lexema: \"" + found.lexeme + "\")";
    }
};

}  // namespace parser
