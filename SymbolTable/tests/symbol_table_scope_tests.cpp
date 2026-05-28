// Fase 1 — C4: tests de scopes (shadowing permitido, redefinición inválida en el mismo scope).
#include <iostream>
#include <string>

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

} // namespace

int main()
{
    bool ok = true;
    SymbolTable table;

    // --- shadowing: inner scope puede redeclarar nombre del padre ---
    ok &= expect(table.declareVariable("x", TypeInfo(TypeInfo::Kind::Number)), "outer scope: declare x");
    table.enterScope();
    ok &= expect(table.declareVariable("x", TypeInfo(TypeInfo::Kind::String)), "inner scope: shadow x with String");
    auto *innerX = table.lookupVariable("x");
    ok &= expect(innerX != nullptr && innerX->type.getKind() == TypeInfo::Kind::String,
                 "lookup resolves to inner x (String)");
    table.exitScope();
    auto *outerX = table.lookupVariable("x");
    ok &= expect(outerX != nullptr && outerX->type.getKind() == TypeInfo::Kind::Number,
                 "after exitScope, lookup resolves to outer x (Number)");

    // --- redefinición inválida en el mismo scope (variables) ---
    table.enterScope();
    ok &= expect(table.declareVariable("y", TypeInfo(TypeInfo::Kind::Boolean)), "declare y once");
    ok &= expect(!table.declareVariable("y", TypeInfo(TypeInfo::Kind::Number)), "reject duplicate y in same scope");
    ok &= expect(table.hasLocalVariable("y"), "y remains the first declaration");
    table.exitScope();

    // --- funciones: no se permiten duplicados globales ---
    ok &= expect(!table.declareFunction("sin", {}, TypeInfo(TypeInfo::Kind::Number)),
                 "reject redeclaring builtin function sin");

    // --- tipos: no se permiten duplicados globales ---
    ok &= expect(!table.declareType("Object", "Object"), "reject redeclaring builtin type Object");
    ok &= expect(table.declareType("Shape", "Object"), "declare user type Shape");
    ok &= expect(!table.declareType("Shape", "Object"), "reject duplicate user type Shape");

    return ok ? 0 : 1;
}
