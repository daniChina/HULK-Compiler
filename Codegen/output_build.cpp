#include "output_build.hpp"

#include <cstdio>
#include <fstream>
#include <sstream>

#include "../SymbolTable/symbol_table.hpp"
#include "llvm_codegen.hpp"

namespace hulk::codegen {
namespace {

constexpr const char* kIrPath = ".hulk_out.ll";
constexpr const char* kRuntimePath = "Codegen/runtime.c";

bool write_text_file(const std::string& path, const std::string& content, std::string* error_out) {
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open()) {
        if (error_out) {
            *error_out = "no se pudo escribir " + path;
        }
        return false;
    }
    out << content;
    return true;
}

bool run_command(const std::string& command, std::string* error_out) {
    const int status = std::system(command.c_str());
    if (status != 0) {
        if (error_out) {
            std::ostringstream oss;
            oss << "fallo al ejecutar: " << command << " (exit " << status << ")";
            *error_out = oss.str();
        }
        return false;
    }
    return true;
}

}  // namespace

bool build_executable(parser::Program* program, const std::string& exe_path, std::string* error_out,
                    const SymbolTable* symbol_table) {
    LLVMCodeGenerator generator;
    generator.initialize("hulk_program");
    if (symbol_table != nullptr) {
        generator.setSymbolTable(symbol_table);
    }
    generator.generate(program);
    if (generator.hadError()) {
        if (error_out) {
            *error_out = generator.lastError();
        }
        return false;
    }

    const std::string ir = generator.getIR();
    if (!write_text_file(kIrPath, ir, error_out)) {
        return false;
    }

    std::remove(exe_path.c_str());

    std::ostringstream cmd;
#if defined(_WIN32)
    cmd << "clang \"" << kIrPath << "\" \"" << kRuntimePath << "\" -o \"" << exe_path
        << "\" -lm 2>&1";
#else
    cmd << "clang \"" << kIrPath << "\" \"" << kRuntimePath << "\" -o \"" << exe_path
        << "\" -lm 2>&1";
#endif
    return run_command(cmd.str(), error_out);
}

}  // namespace hulk::codegen
