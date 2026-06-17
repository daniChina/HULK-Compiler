#pragma once

#include <string>

namespace hulk {

// Escribe .hulk_program.hulk y compila ./output con g++ (contrato matcom).
// Devuelve false si no se pudo invocar el linker (p. ej. en Windows sin toolchain Unix).
bool build_output_executable(const std::string& program_source);

}  // namespace hulk
