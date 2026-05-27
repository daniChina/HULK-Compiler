#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../../Types/type_info.hpp"
#include "../../SymbolTable/symbol_table.hpp"
#include "../../Parser/ast/expr.hpp"
#include "../../Parser/core/token.hpp"

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

parser::Token identToken(const std::string &name)
{
    return parser::Token{parser::TokenType::IDENTIFIER, name, 1, 1};
}

void registerNominalType(SymbolTable &symbolTable, const std::string &name, const std::string &parent)
{
    symbolTable.declareType(name, parent);

    std::optional<parser::Token> parentToken = std::nullopt;
    if (parent != "Object")
    {
        parentToken = identToken(parent);
    }

    auto typeDecl = std::make_unique<parser::TypeDecl>(
        identToken(name),
        std::vector<std::pair<parser::Token, std::optional<parser::Token>>>{},
        parentToken,
        std::vector<parser::ExprPtr>{},
        std::vector<parser::AttributeDef>{},
        std::vector<parser::MethodDef>{});

    symbolTable.storeTypeDeclaration(name, typeDecl.get());

    // Keep declaration alive for the full test execution.
    static std::vector<std::unique_ptr<parser::TypeDecl>> typeDeclStorage;
    typeDeclStorage.push_back(std::move(typeDecl));
}

} // namespace

int main()
{
    SymbolTable symbolTable;
    TypeInfo::setSymbolTable(&symbolTable);

    registerNominalType(symbolTable, "Animal", "Object");
    registerNominalType(symbolTable, "Perro", "Animal");
    registerNominalType(symbolTable, "Gato", "Animal");

    bool ok = true;

    // --- conformsTo: builtins y casos borde ---
    ok &= expect(TypeInfo::Number().conformsTo(TypeInfo::Number()), "Number <= Number");
    ok &= expect(!TypeInfo::Number().conformsTo(TypeInfo::String()), "Number </= String");
    ok &= expect(TypeInfo(TypeInfo::Kind::Unknown).conformsTo(TypeInfo::Number()), "Unknown <= Number");
    ok &= expect(TypeInfo::Number().conformsTo(TypeInfo(TypeInfo::Kind::Unknown)), "Number <= Unknown");
    ok &= expect(TypeInfo(TypeInfo::Kind::Void).conformsTo(TypeInfo(TypeInfo::Kind::Void)), "Void <= Void");
    ok &= expect(!TypeInfo(TypeInfo::Kind::Void).conformsTo(TypeInfo::Object()), "Void </= Object");
    ok &= expect(TypeInfo(TypeInfo::Kind::Null).conformsTo(TypeInfo::Object()), "Null <= Object");
    ok &= expect(TypeInfo(TypeInfo::Kind::Null).conformsTo(TypeInfo::Object("Animal")), "Null <= Animal(Object nominal)");

    // --- subtipado nominal ---
    ok &= expect(TypeInfo::Object("Perro").conformsTo(TypeInfo::Object("Animal")), "Perro <= Animal");
    ok &= expect(!TypeInfo::Object("Animal").conformsTo(TypeInfo::Object("Perro")), "Animal </= Perro");

    // --- LCA ---
    ok &= expect(
        TypeInfo::getLowestCommonAncestor({TypeInfo::Number(), TypeInfo::Number()}).toString() == "Number",
        "LCA(Number, Number) = Number");
    ok &= expect(
        TypeInfo::getLowestCommonAncestor({TypeInfo::Number(), TypeInfo::String()}).toString() == "Object",
        "LCA(Number, String) = Object");
    ok &= expect(
        TypeInfo::getLowestCommonAncestor({TypeInfo(TypeInfo::Kind::Unknown), TypeInfo::Number()}).toString() == "Unknown",
        "LCA(Unknown, Number) = Unknown");
    ok &= expect(
        TypeInfo::getLowestCommonAncestor({TypeInfo(TypeInfo::Kind::Void), TypeInfo::Number()}).toString() == "Unknown",
        "LCA(Void, Number) = Unknown");
    ok &= expect(
        TypeInfo::getLowestCommonAncestor({TypeInfo::Object("Perro"), TypeInfo::Object("Gato")}).toString() == "Animal",
        "LCA(Perro, Gato) = Animal");

    return ok ? 0 : 1;
}
