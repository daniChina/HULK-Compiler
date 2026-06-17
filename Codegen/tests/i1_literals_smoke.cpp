#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"
#include "../llvm_codegen.hpp"

namespace {

parser::Token token(parser::TokenType type, const std::string& lexeme) {
    return parser::Token{type, lexeme, 1, 1};
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

bool checkLiteralIR(parser::Program* program, const std::vector<std::string>& needles, const char* label) {
    hulk::codegen::LLVMCodeGenerator generator;
    generator.initialize("hulk_i1_smoke");
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

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::NumberExpr>(token(parser::TokenType::NUMBER_LITERAL, "42"))).get(),
        {"double 4.200000e+01"},
        "I1.1: literal numerico");

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::StringExpr>(token(parser::TokenType::STRING_LITERAL, "hi"))).get(),
        {R"(c"hi\00")", "BoxedValue"},
        "I1.2: literal string");

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::BoolExpr>(token(parser::TokenType::TRUE, "true"), true)).get(),
        {"i1 true"},
        "I1.3: literal bool true");

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::BoolExpr>(token(parser::TokenType::FALSE, "false"), false)).get(),
        {"i1 false"},
        "I1.3: literal bool false");

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::NullExpr>(token(parser::TokenType::NULL_LITERAL, "Null"))).get(),
        {"store i8* null"},
        "I1: literal Null");

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, "PI"))).get(),
        {"@PI", "load double"},
        "I1: builtin PI");

    ok &= checkLiteralIR(
        programWithExpr(std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, "E"))).get(),
        {"@E", "load double"},
        "I1: builtin E");

    if (ok) {
        std::fprintf(stderr, "[OK] I1 smoke: literales y constantes globales\n");
    }
    return ok ? 0 : 1;
}
