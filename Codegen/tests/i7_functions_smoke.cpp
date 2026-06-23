#include <array>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"
#include "../../SymbolTable/symbol_table.hpp"
#include "../../Types/type_info.hpp"
#include "../llvm_codegen.hpp"
#include "../output_build.hpp"

namespace {

parser::Token token(parser::TokenType type, const std::string& lexeme) {
    return parser::Token{type, lexeme, 1, 1};
}

parser::ExprPtr numberLiteral(const std::string& value) {
    return std::make_unique<parser::NumberExpr>(token(parser::TokenType::NUMBER_LITERAL, value));
}

parser::ExprPtr stringLiteral(const std::string& value) {
    return std::make_unique<parser::StringExpr>(token(parser::TokenType::STRING_LITERAL, value));
}

parser::ExprPtr identifier(const std::string& name) {
    return std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr binary(parser::ExprPtr left, parser::TokenType op_type, const std::string& op_lexeme,
                       parser::ExprPtr right) {
    return std::make_unique<parser::BinaryExpr>(
        std::move(left), token(op_type, op_lexeme), std::move(right));
}

parser::ExprPtr ifExpr(parser::ExprPtr cond, parser::ExprPtr then_branch, parser::ExprPtr else_branch) {
    return std::make_unique<parser::IfExpr>(std::move(cond), std::move(then_branch), std::move(else_branch));
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

parser::StmtPtr functionDecl(const std::string& name, std::vector<std::string> params, parser::ExprPtr body) {
    std::vector<std::pair<parser::Token, std::optional<parser::Token>>> param_tokens;
    param_tokens.reserve(params.size());
    for (const auto& param : params) {
        param_tokens.emplace_back(token(parser::TokenType::IDENTIFIER, param), std::nullopt);
    }
    return std::make_unique<parser::FunctionDecl>(
        token(parser::TokenType::IDENTIFIER, name),
        std::move(param_tokens),
        std::nullopt,
        std::move(body));
}

std::unique_ptr<parser::Program> programWithStmts(std::vector<parser::StmtPtr> stmts) {
    return std::make_unique<parser::Program>(std::move(stmts));
}

std::unique_ptr<parser::Program> programIncrementF() {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(functionDecl(
        "f",
        {"x"},
        binary(identifier("x"), parser::TokenType::PLUS, "+", numberLiteral("1"))));
    std::vector<parser::ExprPtr> call_args;
    call_args.push_back(numberLiteral("2"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(callExpr("f", std::move(call_args)))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programFactorial() {
    std::vector<parser::StmtPtr> stmts;
    std::vector<parser::ExprPtr> fact_arg;
    fact_arg.push_back(binary(identifier("n"), parser::TokenType::MINUS, "-", numberLiteral("1")));
    stmts.push_back(functionDecl(
        "fact",
        {"n"},
        ifExpr(
            binary(identifier("n"), parser::TokenType::LESS_EQUAL, "<=", numberLiteral("1")),
            numberLiteral("1"),
            binary(
                identifier("n"),
                parser::TokenType::STAR,
                "*",
                callExpr("fact", std::move(fact_arg))))));
    std::vector<parser::ExprPtr> fact_call_args;
    fact_call_args.push_back(numberLiteral("5"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(
        printCall(callExpr("fact", std::move(fact_call_args)))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programOverload() {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(functionDecl("f", {"x"}, identifier("x")));
    stmts.push_back(functionDecl(
        "f",
        {"x", "y"},
        binary(identifier("x"), parser::TokenType::PLUS, "+", identifier("y"))));
    std::vector<parser::ExprPtr> args;
    args.push_back(numberLiteral("3"));
    args.push_back(numberLiteral("4"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(callExpr("f", std::move(args)))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programStringFn() {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(functionDecl(
        "tag",
        {"s"},
        binary(stringLiteral("x"), parser::TokenType::CONCAT, "@", identifier("s"))));
    std::vector<parser::ExprPtr> args;
    args.push_back(stringLiteral("hi"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(callExpr("tag", std::move(args)))));
    return programWithStmts(std::move(stmts));
}

SymbolTable symbolTableForStringFn() {
    SymbolTable table;
    table.declareFunction("tag", {TypeInfo::String()}, TypeInfo::String());
    return table;
}

bool expectContains(const std::string& haystack, const std::string& needle, const char* label) {
    if (haystack.find(needle) == std::string::npos) {
        std::fprintf(stderr, "[FAIL] %s: falta \"%s\" en IR\n", label, needle.c_str());
        return false;
    }
    std::fprintf(stderr, "[OK] %s\n", label);
    return true;
}

bool checkIR(parser::Program* program, const std::vector<std::string>& needles, const char* label,
             const SymbolTable* symbol_table = nullptr) {
    hulk::codegen::LLVMCodeGenerator generator;
    generator.initialize("hulk_i7_smoke");
    if (symbol_table != nullptr) {
        generator.setSymbolTable(symbol_table);
    }
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
                     const std::string& exe_path, const SymbolTable* symbol_table = nullptr) {
    std::string error;
    if (!hulk::codegen::build_executable(program, exe_path, &error, symbol_table)) {
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
const char* kExeName = "llvm_i7_out.exe";
#else
const char* kExeName = "llvm_i7_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    const std::string exe = kExeName;

    ok &= checkIR(
        programIncrementF().get(),
        {"define ptr @hulk_fn_f_1", "call ptr @hulk_fn_f_1"},
        "I7.1: define y call de funcion global");

    ok &= runPrintProgram(
        programIncrementF().get(),
        "3\n",
        "I7.2: E2E print(f(2)) (L7.1 / V5.1)",
        exe);

    ok &= runPrintProgram(
        programFactorial().get(),
        "120\n",
        "I7.3: E2E recursion fact(5) (L7.2 / V5.2)",
        exe);

    ok &= runPrintProgram(
        programOverload().get(),
        "7\n",
        "I7.4: E2E overload por aridad f(3, 4)",
        exe);

    SymbolTable string_sym = symbolTableForStringFn();
    ok &= checkIR(
        programStringFn().get(),
        {"define ptr @hulk_fn_tag_1(ptr", "call ptr @hulk_fn_tag_1"},
        "I7.5: funcion String multi-tipo",
        &string_sym);

    ok &= runPrintProgram(
        programStringFn().get(),
        "xhi\n",
        "I7.6: E2E funcion String tag(\"hi\")",
        exe,
        &string_sym);

    if (ok) {
        std::fprintf(stderr, "[OK] I7 smoke: funciones globales LLVM\n");
    }
    return ok ? 0 : 1;
}
