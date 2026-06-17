#pragma once

#include <string>

namespace parser {
struct Program;
}

namespace hulk::codegen {

bool build_executable(parser::Program* program, const std::string& exe_path, std::string* error_out);

}  // namespace hulk::codegen
