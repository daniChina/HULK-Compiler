#include "output_gen.hpp"

#ifdef HULK_HAVE_LLVM
#include "../Codegen/output_build.hpp"
#endif

namespace hulk {

bool build_output_executable(const std::string& /*program_source*/, parser::Program* program,
                             std::string* error_out) {
#ifdef HULK_HAVE_LLVM
    return codegen::build_executable(program, "output", error_out);
#else
    if (error_out) {
        *error_out = "codegen LLVM no disponible: instala llvm-21, pon llvm-config en PATH y ejecuta make compile (ver REQUIREMENTS.md)";
    }
    return false;
#endif
}

}  // namespace hulk
