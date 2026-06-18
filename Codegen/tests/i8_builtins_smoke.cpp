#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"
#include "../llvm_codegen.hpp"
#include "../output_build.hpp"

namespace {

parser::Token token(parser::TokenType type, const std::string& lexeme) {
    return parser::Token{type, lexeme, 1, 1};
}

parser::ExprPtr numberLiteral(const std::string& value) {
    return std::make_unique<parser::NumberExpr>(token(parser::TokenType::NUMBER_LITERAL, value));
}

parser::ExprPtr identifier(const std::string& name) {
    return std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr callExpr(const std::string& name, std::vector<parser::ExprPtr> args) {
    return std::make_unique<parser::CallExpr>(
        identifier(name), token(parser::TokenType::LPAREN, "("), std::move(args));
}

parser::ExprPtr printCall(parser::ExprPtr arg) {
    std::vector<parser::ExprPtr> args;
    args.push_back(std::move(arg));
    return callExpr("print", std::move(args));
}

parser::ExprPtr forExpr(const std::string& var, parser::ExprPtr iterable, parser::ExprPtr body) {
    return std::make_unique<parser::ForExpr>(
        token(parser::TokenType::IDENTIFIER, var),
        std::nullopt,
        std::move(iterable),
        std::move(body));
}

std::unique_ptr<parser::Program> programWithExpr(parser::ExprPtr expr) {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(std::make_unique<parser::ExprStmt>(std::move(expr)));
    return std::make_unique<parser::Program>(std::move(stmts));
}

std::unique_ptr<parser::Program> programSinZero() {
    std::vector<parser::ExprPtr> args;
    args.push_back(numberLiteral("0"));
    return programWithExpr(printCall(callExpr("sin", std::move(args))));
}

std::unique_ptr<parser::Program> programRangeFor() {
    std::vector<parser::ExprPtr> range_args;
    range_args.push_back(numberLiteral("0"));
    range_args.push_back(numberLiteral("3"));
    parser::ExprPtr iterable = callExpr("range", std::move(range_args));
    parser::ExprPtr body = printCall(identifier("i"));
    return programWithExpr(forExpr("i", std::move(iterable), std::move(body)));
}

bool expectContains(const std::string& haystack, const std::string& needle, const char* label) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "[FAIL] %s: falta \"%s\" en IR\n", label, needle.c_str());
        return false;
    }
    std::fprintf(stderr, "[OK] %s\n", label);
    return true;
}

bool checkIR(parser::Program* program, const std::vector<std::string>& needles, const char* label) {
    hulk::codegen::LLVMCodeGenerator generator;
    generator.initialize("hulk_i8_smoke");
    generator.generate(program);

    if (generator.hadError()) {
        std::fprintf(stderr, "[FAIL] %s: codegen error: %s\n", label, generator.lastError().c_str());
        return false;
    }

    const std::string ir = generator.getIR();
    bool ok = true;
    for (const auto& needle : needles) {
        ok &= expectContains(ir, needle, label);
    }
    if (!ok) {
        std::fprintf(stderr, "--- IR generado (%s) ---\n%s\n", label, ir.c_str());
    }
    return ok;
}

std::string captureProcessOutput(const std::string& command) {
    std::array<char, 256> buffer{};
    std::string output;
#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (pipe == nullptr) {
        return {};
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return output;
}

bool runPrintProgram(parser::Program* program, const std::string& expected_stdout, const char* label,
                     const std::string& exe_path) {
    std::string error;
    if (!hulk::codegen::build_executable(program, exe_path, &error)) {
        std::fprintf(stderr, "[FAIL] %s: build error: %s\n", label, error.c_str());
        return false;
    }

    const std::string output = captureProcessOutput("\"" + exe_path + "\"");
    if (output != expected_stdout) {
        std::fprintf(stderr, "[FAIL] %s: stdout esperado %zu bytes, obtuvo %zu\n", label,
                     expected_stdout.size(), output.size());
        std::fprintf(stderr, "  esperado: %s", expected_stdout.c_str());
        std::fprintf(stderr, "  obtuvo:   %s", output.c_str());
        return false;
    }

    std::fprintf(stderr, "[OK] %s\n", label);
    return true;
}

#if defined(_WIN32)
const char* kExeName = "llvm_i8_out.exe";
#else
const char* kExeName = "llvm_i8_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    const std::string exe = kExeName;

    ok &= checkIR(
        programSinZero().get(),
        {"call double @sin"},
        "I8.1: builtin sin en IR");

    ok &= runPrintProgram(
        programSinZero().get(),
        "0\n",
        "I8.2: E2E print(sin(0)) (L8.1 / V6.1)",
        exe);

    ok &= checkIR(
        programRangeFor().get(),
        {"for.cond", "for.body", "call void @hulk_print_double"},
        "I8.3: for-in sobre range en IR");

    ok &= runPrintProgram(
        programRangeFor().get(),
        "0\n1\n2\n",
        "I8.4: E2E for (i in range(0,3)) print(i) (V6.2)",
        exe);

    if (ok) {
        std::fprintf(stderr, "[OK] I8 smoke: builtins math y range LLVM\n");
    }
    return ok ? 0 : 1;
}
