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

parser::ExprPtr boolLiteral(bool value) {
    return std::make_unique<parser::BoolExpr>(
        token(value ? parser::TokenType::TRUE : parser::TokenType::FALSE, value ? "true" : "false"),
        value);
}

parser::ExprPtr identifier(const std::string& name) {
    return std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr ifExpr(parser::ExprPtr cond, parser::ExprPtr then_branch, parser::ExprPtr else_branch) {
    return std::make_unique<parser::IfExpr>(std::move(cond), std::move(then_branch), std::move(else_branch));
}

parser::ExprPtr whileExpr(parser::ExprPtr cond, parser::ExprPtr body, parser::ExprPtr else_branch = nullptr) {
    return std::make_unique<parser::WhileExpr>(std::move(cond), std::move(body), std::move(else_branch));
}

parser::ExprPtr blockExpr(std::vector<parser::ExprPtr> exprs) {
    return std::make_unique<parser::BlockExpr>(std::move(exprs));
}

parser::ExprPtr printCall(parser::ExprPtr arg) {
    std::vector<parser::ExprPtr> args;
    if (arg) {
        args.push_back(std::move(arg));
    }
    return std::make_unique<parser::CallExpr>(
        identifier("print"),
        token(parser::TokenType::LPAREN, "("),
        std::move(args));
}

std::unique_ptr<parser::Program> programWithExpr(parser::ExprPtr expr) {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(std::make_unique<parser::ExprStmt>(std::move(expr)));
    return std::make_unique<parser::Program>(std::move(stmts));
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
    generator.initialize("hulk_i5_smoke");
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
const char* kExeName = "llvm_i5_out.exe";
#else
const char* kExeName = "llvm_i5_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    const std::string exe = kExeName;

    ok &= checkIR(
        programWithExpr(ifExpr(boolLiteral(true), numberLiteral("1"), numberLiteral("2"))).get(),
        {"if.then:", "if.else:", "if.end:", "phi double"},
        "I5.1: if con ramas y phi");

    ok &= checkIR(
        programWithExpr(whileExpr(boolLiteral(false), numberLiteral("1"), numberLiteral("2"))).get(),
        {"while.cond:", "while.body:", "while.else:", "while.end:"},
        "I5.2: while con else");

    ok &= checkIR(
        programWithExpr([&]() {
            std::vector<parser::ExprPtr> exprs;
            exprs.push_back(numberLiteral("1"));
            exprs.push_back(numberLiteral("2"));
            exprs.push_back(numberLiteral("3"));
            return blockExpr(std::move(exprs));
        }())
            .get(),
        {"double 3.000000e+00"},
        "I5.3: bloque devuelve ultima expresion");

    ok &= runPrintProgram(
        programWithExpr(printCall(ifExpr(boolLiteral(true), numberLiteral("1"), numberLiteral("2")))).get(),
        "1\n",
        "I5.4: E2E print(if true 1 else 2) (L5.1)",
        exe);

    ok &= runPrintProgram(
        programWithExpr(printCall(whileExpr(boolLiteral(false), numberLiteral("1"), numberLiteral("2")))).get(),
        "2\n",
        "I5.5: E2E print(while false 1 else 2) (L5.2)",
        exe);

    if (ok) {
        std::fprintf(stderr, "[OK] I5 smoke: if / while / bloques LLVM\n");
    }
    return ok ? 0 : 1;
}
