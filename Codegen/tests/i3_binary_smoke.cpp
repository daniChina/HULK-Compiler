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

parser::ExprPtr boolLiteral(bool value) {
    return std::make_unique<parser::BoolExpr>(
        token(value ? parser::TokenType::TRUE : parser::TokenType::FALSE, value ? "true" : "false"),
        value);
}

parser::ExprPtr identifier(const std::string& name) {
    return std::make_unique<parser::IdentifierExpr>(token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr binary(parser::ExprPtr left, parser::TokenType op_type, const std::string& op_lexeme,
                       parser::ExprPtr right) {
    return std::make_unique<parser::BinaryExpr>(
        std::move(left), token(op_type, op_lexeme), std::move(right));
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

bool checkBinaryIR(parser::Program* program, const std::vector<std::string>& needles, const char* label) {
    hulk::codegen::LLVMCodeGenerator generator;
    generator.initialize("hulk_i3_smoke");
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

    ok &= checkBinaryIR(
        programWithExpr(makeLet(
                            "a",
                            numberLiteral("1"),
                            makeLet("b", numberLiteral("2"), binary(identifier("a"), parser::TokenType::PLUS, "+", identifier("b")))))
            .get(),
        {"fadd double"},
        "I3.1: suma a + b via let");

    ok &= checkBinaryIR(
        programWithExpr(makeLet(
                            "a",
                            numberLiteral("6"),
                            makeLet(
                                "b",
                                numberLiteral("2"),
                                makeLet(
                                    "c",
                                    numberLiteral("2"),
                                    binary(
                                        identifier("a"),
                                        parser::TokenType::MINUS,
                                        "-",
                                        binary(identifier("b"), parser::TokenType::STAR, "*", identifier("c")))))))
            .get(),
        {"fmul double", "fsub double"},
        "I3.2: precedencia a - b * c via let");

    ok &= checkBinaryIR(
        programWithExpr(binary(numberLiteral("7"), parser::TokenType::PERCENT, "%", numberLiteral("3"))).get(),
        {"call double @fmod"},
        "I3.3: modulo 7 % 3");

    ok &= checkBinaryIR(
        programWithExpr(binary(numberLiteral("2"), parser::TokenType::CARET, "^", numberLiteral("3"))).get(),
        {"call double @pow"},
        "I3.4: potencia 2 ^ 3");

    ok &= checkBinaryIR(
        programWithExpr(
            makeLet("x", numberLiteral("1"), binary(identifier("x"), parser::TokenType::PLUS, "+", numberLiteral("1"))))
            .get(),
        {"load double", "fadd double"},
        "I3.5: let x = 1 in x + 1");

    ok &= checkBinaryIR(
        programWithExpr(binary(boolLiteral(true), parser::TokenType::AND, "and", boolLiteral(false))).get(),
        {"land.rhs", "land.end", "phi i1"},
        "I3.6: and logico con cortocircuito");

    ok &= checkBinaryIR(
        programWithExpr(binary(boolLiteral(true), parser::TokenType::OR, "or", boolLiteral(false))).get(),
        {"lor.rhs", "lor.end", "phi i1"},
        "I3.7: or logico con cortocircuito");

    ok &= checkBinaryIR(
        programWithExpr(makeLet(
                            "a",
                            numberLiteral("3"),
                            makeLet(
                                "b",
                                numberLiteral("5"),
                                binary(identifier("a"), parser::TokenType::LESS, "<", identifier("b")))))
            .get(),
        {"fcmp olt double"},
        "I3.8: comparacion numerica < via let");

    if (ok) {
        std::fprintf(stderr, "[OK] I3 smoke: aritmetica binaria LLVM\n");
    }
    return ok ? 0 : 1;
}
