#include <array>
#include <cstdio>
#include <memory>
#include <optional>
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

parser::ExprPtr getAttr(parser::ExprPtr object, const std::string& name) {
    return std::make_unique<parser::GetAttrExpr>(
        std::move(object), token(parser::TokenType::DOT, "."), token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr assignExpr(parser::ExprPtr lhs, parser::ExprPtr rhs) {
    return std::make_unique<parser::AssignExpr>(
        std::move(lhs), token(parser::TokenType::ASSIGN, ":="), std::move(rhs));
}

parser::ExprPtr blockExpr(std::vector<parser::ExprPtr> exprs) {
    return std::make_unique<parser::BlockExpr>(std::move(exprs));
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

parser::ExprPtr newExpr(const std::string& type_name, std::vector<parser::ExprPtr> args) {
    return std::make_unique<parser::NewExpr>(token(parser::TokenType::IDENTIFIER, type_name), std::move(args));
}

parser::ExprPtr methodCall(parser::ExprPtr object, const std::string& method, std::vector<parser::ExprPtr> args) {
    return std::make_unique<parser::CallExpr>(
        getAttr(std::move(object), method),
        token(parser::TokenType::LPAREN, "("),
        std::move(args));
}

parser::ExprPtr printCall(parser::ExprPtr arg) {
    std::vector<parser::ExprPtr> args;
    args.push_back(std::move(arg));
    return std::make_unique<parser::CallExpr>(
        identifier("print"),
        token(parser::TokenType::LPAREN, "("),
        std::move(args));
}

parser::StmtPtr classDecl(const std::string& name, std::vector<std::string> params,
                          std::vector<std::pair<std::string, parser::ExprPtr>> attrs,
                          std::vector<std::pair<std::string, parser::ExprPtr>> methods) {
    std::vector<std::pair<parser::Token, std::optional<parser::Token>>> param_tokens;
    param_tokens.reserve(params.size());
    for (const auto& param : params) {
        param_tokens.emplace_back(token(parser::TokenType::IDENTIFIER, param), std::nullopt);
    }

    std::vector<parser::AttributeDef> attribute_defs;
    attribute_defs.reserve(attrs.size());
    for (auto& [attr_name, value] : attrs) {
        attribute_defs.push_back(
            parser::AttributeDef{token(parser::TokenType::IDENTIFIER, attr_name), std::nullopt, std::move(value)});
    }

    std::vector<parser::MethodDef> method_defs;
    method_defs.reserve(methods.size());
    for (auto& [method_name, body] : methods) {
        method_defs.push_back(parser::MethodDef{
            token(parser::TokenType::IDENTIFIER, method_name),
            {},
            std::nullopt,
            std::move(body)});
    }

    return std::unique_ptr<parser::ClassDecl>(new parser::ClassDecl(
        token(parser::TokenType::IDENTIFIER, name),
        std::move(param_tokens),
        std::nullopt,
        std::vector<parser::ExprPtr>{},
        std::move(attribute_defs),
        std::move(method_defs)));
}

std::unique_ptr<parser::Program> programWithStmts(std::vector<parser::StmtPtr> stmts) {
    return std::make_unique<parser::Program>(std::move(stmts));
}

std::unique_ptr<parser::Program> programNewAttrRead() {
    std::vector<parser::StmtPtr> stmts;
    std::vector<std::pair<std::string, parser::ExprPtr>> point_attrs;
    point_attrs.emplace_back("x", identifier("x"));
    point_attrs.emplace_back("y", identifier("y"));
    stmts.push_back(classDecl(
        "Point",
        {"x", "y"},
        std::move(point_attrs),
        {}));

    std::vector<parser::ExprPtr> ctor_args;
    ctor_args.push_back(numberLiteral("1"));
    ctor_args.push_back(numberLiteral("2"));
    parser::ExprPtr body = getAttr(identifier("p"), "x");
    stmts.push_back(std::make_unique<parser::ExprStmt>(
        printCall(makeLet("p", newExpr("Point", std::move(ctor_args)), std::move(body)))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programAttrAssign() {
    std::vector<parser::StmtPtr> stmts;
    std::vector<std::pair<std::string, parser::ExprPtr>> point_attrs;
    point_attrs.emplace_back("x", identifier("x"));
    stmts.push_back(classDecl(
        "Point",
        {"x"},
        std::move(point_attrs),
        {}));

    std::vector<parser::ExprPtr> ctor_args;
    ctor_args.push_back(numberLiteral("0"));
    std::vector<parser::ExprPtr> block_exprs;
    block_exprs.push_back(assignExpr(getAttr(identifier("p"), "x"), numberLiteral("5")));
    block_exprs.push_back(getAttr(identifier("p"), "x"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(makeLet(
        "p",
        newExpr("Point", std::move(ctor_args)),
        blockExpr(std::move(block_exprs))))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programMethodCall() {
    std::vector<parser::StmtPtr> stmts;
    std::vector<parser::ExprPtr> inc_body_exprs;
    inc_body_exprs.push_back(assignExpr(
        getAttr(std::make_unique<parser::SelfExpr>(token(parser::TokenType::SELF, "self")), "n"),
        binary(getAttr(std::make_unique<parser::SelfExpr>(token(parser::TokenType::SELF, "self")), "n"),
               parser::TokenType::PLUS,
               "+",
               numberLiteral("1"))));

    std::vector<std::pair<std::string, parser::ExprPtr>> counter_attrs;
    counter_attrs.emplace_back("n", identifier("n"));
    std::vector<std::pair<std::string, parser::ExprPtr>> methods;
    methods.emplace_back("inc", blockExpr(std::move(inc_body_exprs)));

    stmts.push_back(classDecl(
        "Counter",
        {"n"},
        std::move(counter_attrs),
        std::move(methods)));

    std::vector<parser::ExprPtr> ctor_args;
    ctor_args.push_back(numberLiteral("0"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(makeLet(
        "c",
        newExpr("Counter", std::move(ctor_args)),
        methodCall(identifier("c"), "inc", {})))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programPrintInstance() {
    std::vector<parser::StmtPtr> stmts;
    std::vector<std::pair<std::string, parser::ExprPtr>> point_attrs;
    point_attrs.emplace_back("x", identifier("x"));
    point_attrs.emplace_back("y", identifier("y"));
    stmts.push_back(classDecl(
        "Point",
        {"x", "y"},
        std::move(point_attrs),
        {}));

    std::vector<parser::ExprPtr> ctor_args;
    ctor_args.push_back(numberLiteral("1"));
    ctor_args.push_back(numberLiteral("2"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(
        printCall(newExpr("Point", std::move(ctor_args)))));
    return programWithStmts(std::move(stmts));
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
    generator.initialize("hulk_i9_smoke");
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
const char* kExeName = "llvm_i9_out.exe";
#else
const char* kExeName = "llvm_i9_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    const std::string exe = kExeName;

    ok &= checkIR(
        programNewAttrRead().get(),
        {"HulkInstance_Point", "malloc"},
        "I9.1: new + attr read IR");

    ok &= runPrintProgram(
        programNewAttrRead().get(),
        "1\n",
        "I9.2: E2E new Point + p.x (C2 / L9.1)",
        exe);

    ok &= runPrintProgram(
        programAttrAssign().get(),
        "5\n",
        "I9.3: E2E p.x := 5 (C3 / L9.2)",
        exe);

    ok &= runPrintProgram(
        programMethodCall().get(),
        "1\n",
        "I9.4: E2E c.inc() + self (C4 / L10.1)",
        exe);

    ok &= checkIR(
        programMethodCall().get(),
        {"@hulk_meth_Counter_inc_0"},
        "I9.5: method call IR");

    ok &= runPrintProgram(
        programPrintInstance().get(),
        "<instance>\n",
        "I9.6: E2E print(new Point(...)) (L8 print instance)",
        exe);

    if (ok) {
        std::fprintf(stderr, "[OK] I9 smoke: OOP type/new/attrs/methods\n");
    }
    return ok ? 0 : 1;
}
