#include "output_gen.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace hulk {
namespace {

constexpr const char* kProgramSidecar = ".hulk_program.hulk";
constexpr const char* kOutputBinary = "output";

std::string shell_quote(const std::string& value) {
    std::string out = "\"";
    for (char ch : value) {
        if (ch == '"') {
            out += "\\\"";
        } else {
            out += ch;
        }
    }
    out += '"';
    return out;
}

std::vector<std::string> runtime_source_files() {
    return {
        "Lexer/hulk_lexer.cpp",
        "Parser/core/token_adapter.cpp",
        "Parser/core/token_stream.cpp",
        "Parser/ast/expr.cpp",
        "Parser/ast/cst_nodes.cpp",
        "Parser/ast/cst_to_ast.cpp",
        "Parser/generator/grammar_reader.cpp",
        "Parser/generator/first_follow.cpp",
        "Parser/generator/ll1_table.cpp",
        "Parser/syntax/ll1_parser.cpp",
        "SemanticCheck/binding_list.cpp",
        "SemanticCheck/phase2_checker.cpp",
        "Types/type_info.cpp",
        "SymbolTable/decl_collector.cpp",
        "Value/value.cpp",
        "Evaluator/evaluator.cpp",
        "Compiler/pipeline.cpp",
        "Compiler/output_main.cpp",
    };
}

std::string default_cxx() {
    const char* from_env = std::getenv("CXX");
    if (from_env != nullptr && from_env[0] != '\0') {
        return from_env;
    }
    return "g++";
}

std::string flex_include_path() {
    const char* from_env = std::getenv("HULK_FLEX_INCLUDE");
    if (from_env != nullptr && from_env[0] != '\0') {
        return from_env;
    }
#ifdef _WIN32
    const char* local_app = std::getenv("LOCALAPPDATA");
    if (local_app != nullptr) {
        return std::string(local_app) +
               "/Microsoft/WinGet/Packages/"
               "WinFlexBison.win_flex_bison_Microsoft.Winget.Source_8wekyb3d8bbwe";
    }
#endif
    return "/usr/include";
}

std::string include_flags() {
    std::ostringstream flags;
    flags << "-std=c++17 -O2 -Wall -I. -ILexer -IParser/core -IParser/ast -IParser/generator "
             "-IParser/syntax -ISemanticCheck -ISymbolTable -ITypes -IValue -IEvaluator "
          << "-I" << flex_include_path();
#ifdef _WIN32
    flags << " -IC:/ghcup/msys64/usr/include -fuse-ld=bfd";
#endif
    return flags.str();
}

}  // namespace

bool build_output_executable(const std::string& program_source) {
    {
        std::ofstream sidecar(kProgramSidecar, std::ios::binary);
        if (!sidecar.is_open()) {
            return false;
        }
        sidecar << program_source;
    }

    std::ostringstream command;
    command << default_cxx() << ' ' << include_flags();
    for (const auto& source : runtime_source_files()) {
#ifdef _WIN32
        command << ' ' << source;
#else
        command << ' ' << shell_quote(source);
#endif
    }
#ifdef _WIN32
    command << " -o " << kOutputBinary << ".exe";
#else
    command << " -o " << shell_quote(kOutputBinary);
#endif

    const int status = std::system(command.str().c_str());
#ifdef _WIN32
    if (status == 0) {
        std::remove(kOutputBinary);
        return std::rename((std::string(kOutputBinary) + ".exe").c_str(), kOutputBinary) == 0;
    }
#endif
    return status == 0;
}

}  // namespace hulk
