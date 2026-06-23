#pragma once

#include <string>

class SymbolTable;

namespace parser {
struct Program;
}

namespace hulk::codegen {

bool build_executable(parser::Program* program, const std::string& exe_path, std::string* error_out,
                    const SymbolTable* symbol_table = nullptr);

}  // namespace hulk::codegen
