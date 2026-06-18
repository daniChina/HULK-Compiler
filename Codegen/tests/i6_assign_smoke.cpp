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

parser::ExprPtr assignExpr(parser::ExprPtr lhs, parser::ExprPtr rhs) {
    return std::make_unique<parser::AssignExpr>(
        std::move(lhs), token(parser::TokenType::ASSIGN, ":="), std::move(rhs));
}

parser::ExprPtr binary(parser::ExprPtr left, parser::TokenType op_type, const std::string& op_lexeme,
                       parser::ExprPtr right) {
    return std::make_unique<parser::BinaryExpr>(
        std::move(left), token(op_type, op_lexeme), std::move(right));
}

parser::ExprPtr blockExpr(std::vector<parser::ExprPtr> exprs) {
    return std::make_unique<parser::BlockExpr>(std::move(exprs));
}

parser::ExprPtr makeLet(const std::string& name, parser::ExprPtr init, parser::ExprPtr body) {
    return std::make_unique<parser::LetExpr>(
        token(parser::TokenType::IDENTIFIER, name),
        std::nullopt,
        std::move(init),
        std::move(body));
}

parser::ExprPtr whileExpr(parser::ExprPtr cond, parser::ExprPtr body) {
    return std::make_unique<parser::WhileExpr>(std::move(cond), std::move(body), nullptr);
}

parser::ExprPtr printCall(parser::ExprPtr arg) {
    std::vector<parser::ExprPtr> args;
    args.push_back(std::move(arg));
    return std::make_unique<parser::CallExpr>(
        identifier("print"),
        token(parser::TokenType::LPAREN, "("),
        std::move(args));
}

parser::ExprPtr assignBlockLetX() {
    std::vector<parser::ExprPtr> exprs;
    exprs.push_back(assignExpr(identifier("x"), numberLiteral("3")));
    exprs.push_back(identifier("x"));
    return blockExpr(std::move(exprs));
}

parser::ExprPtr whileCounterBody() {
    std::vector<parser::ExprPtr> exprs;
    exprs.push_back(assignExpr(
        identifier("i"),
        binary(identifier("i"), parser::TokenType::PLUS, "+", numberLiteral("1"))));
    return blockExpr(std::move(exprs));
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
    generator.initialize("hulk_i6_smoke");
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
const char* kExeName = "llvm_i6_out.exe";
#else
const char* kExeName = "llvm_i6_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    const std::string exe = kExeName;

    ok &= checkIR(
        programWithExpr(makeLet("x", numberLiteral("0"), assignBlockLetX())).get(),
        {"store double 3.000000e+00", "load double"},
        "I6.1: := escribe en alloca del let");

    ok &= runPrintProgram(
        programWithExpr(printCall(makeLet("x", numberLiteral("0"), assignBlockLetX()))).get(),
        "3\n",
        "I6.2: E2E print(let x = 0 in { x := 3; x; }) (L6.1)",
        exe);

    ok &= runPrintProgram(
        programWithExpr(printCall(makeLet(
                                      "i",
                                      numberLiteral("0"),
                                      whileExpr(
                                          binary(identifier("i"), parser::TokenType::LESS, "<", numberLiteral("3")),
                                          whileCounterBody()))))
            .get(),
        "3\n",
        "I6.3: E2E while con i := i + 1 (V4.2 / eval_loops)",
        exe);

    if (ok) {
        std::fprintf(stderr, "[OK] I6 smoke: asignacion ':=' LLVM\n");
    }
    return ok ? 0 : 1;
}
