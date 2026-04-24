#pragma once

#include <stdexcept>
#include <string>

#include "token.hpp"

namespace parser {

class ParseError : public std::runtime_error {
public:
    ParseError(const Token& found, const std::string& message)
        : std::runtime_error(build_message(found, message)),
          found_(found) {}

    const Token& found() const { return found_; }

private:
    Token found_;

    static std::string build_message(const Token& found, const std::string& message) {
        return "Error de parseo en linea " + std::to_string(found.line) +
               ", columna " + std::to_string(found.col) +
               ": " + message +
               " (token encontrado: " + token_name(found.type) +
               ", lexema: \"" + found.lexeme + "\")";
    }
};

}  // namespace parser
