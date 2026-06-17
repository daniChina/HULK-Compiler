#pragma once

#include <string>

namespace parser {
struct Program;
}

namespace hulk {

// Fase 4: generará ./output vía IR LLVM + clang + runtime C (ver plan maestro).
bool build_output_executable(const std::string& program_source, parser::Program* program,
                             std::string* error_out = nullptr);

}  // namespace hulk
