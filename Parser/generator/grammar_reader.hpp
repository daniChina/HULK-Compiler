#pragma once

#include <iosfwd>
#include <string>

#include "production.hpp"

namespace parser::generator {

Grammar read_grammar(std::istream& input);
Grammar read_grammar_file(const std::string& path);

}  // namespace parser::generator
