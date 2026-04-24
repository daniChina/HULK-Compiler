#pragma once

#include <iosfwd>
#include <string>

#include "token.hpp"

class HulkLexer;
enum class TokenType : int;

namespace parser {

TokenType from_lexer_token(::TokenType lexer_type);
Token make_token(const HulkLexer& lexer, ::TokenType lexer_type);
TokenList tokenize_stream(std::istream& input);
TokenList tokenize_file(const std::string& path);

}  // namespace parser
