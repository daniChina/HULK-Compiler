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

parser::ExprPtr selfExpr() {
    return std::make_unique<parser::SelfExpr>(token(parser::TokenType::SELF, "self"));
}

parser::ExprPtr getAttr(parser::ExprPtr object, const std::string& name) {
    return std::make_unique<parser::GetAttrExpr>(
        std::move(object), token(parser::TokenType::DOT, "."), token(parser::TokenType::IDENTIFIER, name));
}

parser::ExprPtr asExpr(parser::ExprPtr object, const std::string& type_name) {
    return std::make_unique<parser::AsExpr>(
        std::move(object), token(parser::TokenType::AS, "as"), token(parser::TokenType::IDENTIFIER, type_name));
}

parser::ExprPtr isExpr(parser::ExprPtr object, const std::string& type_name) {
    return std::make_unique<parser::IsExpr>(
        std::move(object), token(parser::TokenType::IS, "is"), token(parser::TokenType::IDENTIFIER, type_name));
}

parser::ExprPtr grouped(parser::ExprPtr expression) {
    return std::make_unique<parser::GroupedExpr>(
        token(parser::TokenType::LPAREN, "("), std::move(expression));
}

parser::ExprPtr makeLet(const std::string& name, std::optional<parser::Token> declared_type,
                        parser::ExprPtr init, parser::ExprPtr body) {
    return std::make_unique<parser::LetExpr>(
        token(parser::TokenType::IDENTIFIER, name),
        std::move(declared_type),
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

parser::Token typeToken(const std::string& name) {
    return token(parser::TokenType::IDENTIFIER, name);
}

parser::StmtPtr classDecl(
    const std::string& name,
    std::vector<std::pair<std::string, std::optional<std::string>>> params,
    std::optional<std::string> parent,
    std::vector<parser::ExprPtr> parent_args,
    std::vector<std::pair<std::string, std::optional<std::string>>> attrs,
    std::vector<std::pair<std::string, parser::ExprPtr>> methods) {
    std::vector<std::pair<parser::Token, std::optional<parser::Token>>> param_tokens;
    param_tokens.reserve(params.size());
    for (const auto& [param_name, param_type] : params) {
        param_tokens.emplace_back(
            token(parser::TokenType::IDENTIFIER, param_name),
            param_type ? std::optional<parser::Token>(typeToken(*param_type)) : std::nullopt);
    }

    std::vector<parser::AttributeDef> attribute_defs;
    attribute_defs.reserve(attrs.size());
    for (const auto& [attr_name, attr_type] : attrs) {
        parser::ExprPtr init = identifier(attr_name);
        attribute_defs.push_back(parser::AttributeDef{
            token(parser::TokenType::IDENTIFIER, attr_name),
            attr_type ? std::optional<parser::Token>(typeToken(*attr_type)) : std::nullopt,
            std::move(init)});
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
        parent ? std::optional<parser::Token>(typeToken(*parent)) : std::nullopt,
        std::move(parent_args),
        std::move(attribute_defs),
        std::move(method_defs)));
}

std::unique_ptr<parser::Program> programWithStmts(std::vector<parser::StmtPtr> stmts) {
    return std::make_unique<parser::Program>(std::move(stmts));
}

std::unique_ptr<parser::Program> programIsSub() {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(classDecl(
        "Person",
        {{"n", "Number"}},
        std::nullopt,
        {},
        {{"n", "Number"}},
        {}));

    std::vector<parser::ExprPtr> parent_args;
    parent_args.push_back(identifier("n"));
    std::vector<std::pair<std::string, parser::ExprPtr>> sub_methods;
    sub_methods.emplace_back(
        "twice",
        std::make_unique<parser::BinaryExpr>(
            getAttr(selfExpr(), "n"),
            token(parser::TokenType::STAR, "*"),
            numberLiteral("2")));
    stmts.push_back(classDecl(
        "Sub",
        {{"n", "Number"}},
        "Person",
        std::move(parent_args),
        {},
        std::move(sub_methods)));

    std::vector<parser::ExprPtr> ctor_args;
    ctor_args.push_back(numberLiteral("5"));
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(makeLet(
        "x",
        std::nullopt,
        newExpr("Sub", std::move(ctor_args)),
        isExpr(identifier("x"), "Sub")))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programAsPolymorphic() {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(classDecl(
        "Person",
        {{"n", "Number"}},
        std::nullopt,
        {},
        {{"n", "Number"}},
        {}));

    std::vector<parser::ExprPtr> parent_args;
    parent_args.push_back(identifier("n"));
    std::vector<std::pair<std::string, parser::ExprPtr>> sub_methods;
    sub_methods.emplace_back(
        "twice",
        std::make_unique<parser::BinaryExpr>(
            getAttr(selfExpr(), "n"),
            token(parser::TokenType::STAR, "*"),
            numberLiteral("2")));
    stmts.push_back(classDecl(
        "Sub",
        {{"n", "Number"}},
        "Person",
        std::move(parent_args),
        {},
        std::move(sub_methods)));

    std::vector<parser::ExprPtr> ctor_args;
    ctor_args.push_back(numberLiteral("5"));
    parser::ExprPtr body = methodCall(
        grouped(asExpr(identifier("v"), "Sub")),
        "twice",
        std::vector<parser::ExprPtr>{});
    stmts.push_back(std::make_unique<parser::ExprStmt>(printCall(makeLet(
        "v",
        typeToken("Person"),
        newExpr("Sub", std::move(ctor_args)),
        std::move(body)))));
    return programWithStmts(std::move(stmts));
}

std::unique_ptr<parser::Program> programScalarIs() {
    std::vector<parser::StmtPtr> stmts;
    stmts.push_back(std::make_unique<parser::ExprStmt>(
        printCall(isExpr(numberLiteral("42"), "Number"))));
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
    generator.initialize("hulk_i11_smoke");
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
const char* kExeName = "llvm_i11_out.exe";
#else
const char* kExeName = "llvm_i11_out";
#endif

}  // namespace

int main() {
    bool ok = true;
    const std::string exe = kExeName;

    ok &= checkIR(
        programIsSub().get(),
        {"icmp", "HulkInstance_Sub"},
        "I11.1: is + runtime type IR");

    ok &= runPrintProgram(
        programIsSub().get(),
        "true\n",
        "I11.2: E2E x is Sub (C7 / L13.1)",
        exe + "_c7");

    ok &= runPrintProgram(
        programAsPolymorphic().get(),
        "10\n",
        "I11.3: E2E let v: Person = new Sub; (v as Sub).twice() (C8)",
        exe + "_c8");

    ok &= runPrintProgram(
        programScalarIs().get(),
        "true\n",
        "I11.4: E2E 42 is Number",
        exe + "_scalar");

    if (ok) {
        std::fprintf(stderr, "[OK] I11 smoke: is / as completo\n");
    }
    return ok ? 0 : 1;
}
