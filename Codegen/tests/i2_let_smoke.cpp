#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"
#include "../llvm_codegen.hpp"

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

parser::ExprPtr makeLet(const std::string& name, parser::ExprPtr init, parser::ExprPtr body) {
    return std::make_unique<parser::LetExpr>(
        token(parser::TokenType::IDENTIFIER, name),
        std::nullopt,
        std::move(init),
        std::move(body));
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

bool checkLetIR(parser::Program* program, const std::vector<std::string>& needles, const char* label) {
    hulk::codegen::LLVMCodeGenerator generator;
    generator.initialize("hulk_i2_smoke");
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

}  // namespace

int main() {
    bool ok = true;

    ok &= checkLetIR(
        programWithExpr(makeLet("x", numberLiteral("42"), identifier("x"))).get(),
        {"alloca double", "store double 4.200000e+01", "load double"},
        "I2.1: let bind + load");

    ok &= checkLetIR(
        programWithExpr(makeLet(
                             "x",
                             numberLiteral("1"),
                             makeLet("x", numberLiteral("2"), identifier("x"))))
            .get(),
        {"store double 1.000000e+00", "store double 2.000000e+00", "load double"},
        "I2.2: let sombra interna");

    ok &= checkLetIR(
        programWithExpr(makeLet(
                             "x",
                             numberLiteral("1"),
                             makeLet("y", identifier("x"), identifier("y"))))
            .get(),
        {"alloca double", "load double", "store double 1.000000e+00"},
        "I2.3: let captura binding externo");

    if (ok) {
        std::fprintf(stderr, "[OK] I2 smoke: let y ambitos LLVM\n");
    }
    return ok ? 0 : 1;
}
