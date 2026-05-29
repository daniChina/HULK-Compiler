// Fase 1 — C3: smoke de SymbolTable (builtins, scope global, recorrido superficial del AST).
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"
#include "../../SymbolTable/decl_collector.hpp"
#include "../../SymbolTable/symbol_table.hpp"
#include "../../Types/type_info.hpp"

namespace {

bool expect(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "[FAIL] " << message << "\n";
        return false;
    }
    std::cout << "[OK] " << message << "\n";
    return true;
}

parser::Token identToken(const std::string &name, int line = 1)
{
    return parser::Token{parser::TokenType::IDENTIFIER, name, line, 1};
}

parser::ProgramPtr makeSampleProgram()
{
    std::vector<parser::StmtPtr> stmts;

    auto squareBody = std::make_unique<parser::IdentifierExpr>(identToken("x"));
    auto square = std::make_unique<parser::FunctionDecl>(
        identToken("square"),
        std::vector<std::pair<parser::Token, std::optional<parser::Token>>>{
            {identToken("x"), std::nullopt}},
        std::nullopt,
        std::move(squareBody));
    stmts.push_back(std::move(square));

    auto pointType = std::make_unique<parser::ClassDecl>(
        identToken("Point"),
        std::vector<std::pair<parser::Token, std::optional<parser::Token>>>{},
        std::nullopt,
        std::vector<parser::ExprPtr>{},
        std::vector<parser::AttributeDef>{},
        std::vector<parser::MethodDef>{});
    stmts.push_back(std::move(pointType));

    auto exprOnly = std::make_unique<parser::ExprStmt>(
        std::make_unique<parser::NumberExpr>(parser::Token{parser::TokenType::NUMBER_LITERAL, "1", 3, 1}));
    stmts.push_back(std::move(exprOnly));

    return std::make_unique<parser::Program>(std::move(stmts));
}

} // namespace

int main()
{
    bool ok = true;

    SymbolTable table;
    TypeInfo::setSymbolTable(&table);

    // --- scope global y builtins (C3) ---
    ok &= expect(table.isVariableDeclared("PI"), "global scope: constant PI is pre-registered");
    ok &= expect(table.isVariableDeclared("E"), "global scope: constant E is pre-registered");
    ok &= expect(table.isFunctionDeclared("print"), "global scope: builtin print is pre-registered");
    ok &= expect(table.isFunctionDeclared("sin"), "global scope: builtin sin is pre-registered");
    ok &= expect(table.isTypeDeclared("Number"), "global scope: builtin type Number is pre-registered");
    ok &= expect(table.isTypeDeclared("Object"), "global scope: builtin type Object is pre-registered");

    auto *pi = table.lookupVariable("PI");
    ok &= expect(pi != nullptr && pi->type.getKind() == TypeInfo::Kind::Number && !pi->is_mutable,
                 "PI lookup returns immutable Number");

    auto printFn = table.lookupFunction("print");
    ok &= expect(printFn != nullptr && printFn->return_type.getKind() == TypeInfo::Kind::Void,
                 "print lookup returns Void-returning function");

    // --- recorrido superficial del AST (C3) ---
    auto program = makeSampleProgram();
    const auto collected = collectTopLevelDeclarations(*program, table);

    ok &= expect(collected.functions_registered == 1, "AST walk registers one top-level function");
    ok &= expect(collected.types_registered == 1, "AST walk registers one top-level type");
    ok &= expect(collected.skipped_stmts == 1, "AST walk skips non-declaration statements");
    ok &= expect(table.isFunctionDeclared("square"), "square is visible after AST collection");
    ok &= expect(table.isTypeDeclared("Point"), "Point is visible after AST collection");
    ok &= expect(table.getTypeDeclaration("Point") != nullptr, "Point ClassDecl pointer is stored for later phases");

    return ok ? 0 : 1;
}
