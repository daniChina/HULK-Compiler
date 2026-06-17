#include <cstdio>
#include <string>

#include "../llvm_codegen.hpp"

namespace {

bool expectContains(const std::string& haystack, const std::string& needle, const char* label) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "[FAIL] %s: falta \"%s\" en IR\n", label, needle.c_str());
        return false;
    }
    std::fprintf(stderr, "[OK] %s\n", label);
    return true;
}

}  // namespace

int main() {
    hulk::codegen::LLVMCodeGenerator generator;
    generator.initialize("hulk_i0_smoke");
    generator.generate(nullptr);

    if (generator.hadError()) {
        std::fprintf(stderr, "[FAIL] codegen error: %s\n", generator.lastError().c_str());
        return 1;
    }

    const std::string ir = generator.getIR();

    bool ok = true;
    ok &= expectContains(ir, "define i32 @main()", "I0: funcion main");
    ok &= expectContains(ir, "ret i32 0", "I0: retorno cero");

    if (!ok) {
        std::fprintf(stderr, "--- IR generado ---\n%s\n", ir.c_str());
        return 1;
    }

    std::fprintf(stderr, "[OK] I0 smoke: modulo LLVM con @main vacio\n");
    return 0;
}
