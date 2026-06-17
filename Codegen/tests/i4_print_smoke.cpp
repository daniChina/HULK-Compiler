#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"
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

parser::ExprPtr stringLiteral(const std::string& value) {
    return std::make_unique<parser::StringExpr>(token(parser::TokenType::STRING_LITERAL, value));
}

parser::ExprPtr identifier(const std::string& name) {
    return std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr nullLiteral() {
    return std::make_unique<parser::NullExpr>(token(parser::TokenType::NULL_LITERAL, "Null"));
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

std::unique_ptr<parser::Program> programWithStmts(std::vector<parser::StmtPtr> stmts) {
    return std::make_unique<parser::Program>(std::move(stmts));
}

std::unique_ptr<parser::Program> programWithPrint(parser::ExprPtr arg) {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(std::move(arg))));
    return programWithStmts(std::move(stmts));
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

std::string formatDoubleLine(double value) {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%g\n", value);
    return buffer;
}

#if defined(_WIN32)
const char* kExeName = "llvm_i4_out.exe";
#else
const char* kExeName = "llvm_i4_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kE = 2.71828182845904523536;
    const std::string exe = kExeName;

    ok &= runPrintProgram(programWithPrint(numberLiteral("42")).get(), "42\n", "I4.1: print numero",
                          exe);
    ok &= runPrintProgram(programWithPrint(numberLiteral("3")).get(), "3\n", "I4.2: print 1+2 folded",
                          exe);
    ok &= runPrintProgram(programWithPrint(identifier("PI")).get(), formatDoubleLine(kPi),
                          "I4.3: print PI", exe);
    ok &= runPrintProgram(programWithPrint(identifier("E")).get(), formatDoubleLine(kE), "I4.4: print E",
                          exe);
    ok &= runPrintProgram(programWithPrint(boolLiteral(true)).get(), "true\n", "I4.5: print true", exe);
    ok &= runPrintProgram(programWithPrint(boolLiteral(false)).get(), "false\n", "I4.6: print false",
                          exe);
    ok &= runPrintProgram(programWithPrint(stringLiteral("hi")).get(), "hi\n", "I4.7: print string", exe);
    ok &= runPrintProgram(programWithPrint(nullLiteral()).get(), "Null\n", "I4.8: print Null", exe);

    {
        std::vector<parser::StmtPtr> stmts;
        stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(identifier("PI"))));
        stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(identifier("E"))));
        ok &= runPrintProgram(programWithStmts(std::move(stmts)).get(), formatDoubleLine(kPi) + formatDoubleLine(kE),
                              "I4.9: print PI + E (L4.1)", exe);
    }

    {
        std::vector<parser::StmtPtr> stmts;
        stmts.push_back(std::make_unique<parser::ExprStmt>(std::make_unique<parser::CallExpr>(
            identifier("print"),
            token(parser::TokenType::LPAREN, "("),
            std::vector<parser::ExprPtr>{})));
        ok &= runPrintProgram(programWithStmts(std::move(stmts)).get(), "\n", "I4.10: print() vacio", exe);
    }

    if (ok) {
        std::fprintf(stderr, "[OK] I4 smoke: print + runtime C + ejecucion nativa\n");
    }
    return ok ? 0 : 1;
}
