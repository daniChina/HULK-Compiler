#include "type_info.hpp"
#include "../SymbolTable/symbol_table.hpp"
#include <algorithm>
#include <set>

bool TypeInfo::conformsTo(const TypeInfo &other) const
{
    // Regla 0: Unknown es compatible en ambos sentidos (modo inferencia gradual).
    if (kind_ == Kind::Unknown || other.getKind() == Kind::Unknown)
    {
        return true;
    }

    // Regla 1: Igualdad exacta de tipo.
    if (kind_ == other.kind_ && typeName_ == other.typeName_)
    {
        return true;
    }

    // Regla 2: Void solo conforma a Void (evitar mezclar "no valor" con tipos valor).
    if (kind_ == Kind::Void || other.getKind() == Kind::Void)
    {
        return kind_ == Kind::Void && other.getKind() == Kind::Void;
    }

    // Regla 3: Null conforma a cualquier tipo objeto (incluyendo Object nominal).
    if (kind_ == Kind::Null && other.getKind() == Kind::Object)
    {
        return true;
    }

    // Regla 4: Todo tipo valor conforma a Object universal.
    if (other.getKind() == Kind::Object && other.getTypeName().empty())
    {
        return true;
    }

    // Regla 5: Si T1 hereda T2 entonces T1 <= T2.
    if (kind_ == Kind::Object && other.kind_ == Kind::Object)
    {
        return isSubtypeOf(other.getTypeName());
    }

    return false;
}

bool TypeInfo::isSubtypeOf(const std::string &baseTypeName) const
{
    if (kind_ != Kind::Object)
        return false;

    if (baseTypeName.empty() || baseTypeName == "Object")
        return true;

    if (typeName_ == baseTypeName)
        return true;

    // Si no tenemos acceso al symbol table, no podemos verificar herencia
    if (!symbolTable_)
        return false;

    auto getParentNameOrObject = [](const TypeDecl* decl) -> std::string {
        if (!decl)
            return "Object";
        // En el AST local, el padre está en `parent_name` (optional<Token>)
        if (decl->parent_name.has_value())
            return decl->parent_name->lexeme;
        return "Object";
    };

    // Buscar en la cadena de herencia
    auto currentType = symbolTable_->getTypeDeclaration(typeName_);
    while (currentType && getParentNameOrObject(currentType) != "Object")
    {
        const auto parent = getParentNameOrObject(currentType);
        if (parent == baseTypeName)
            return true;
        currentType = symbolTable_->getTypeDeclaration(parent);
    }

    return false;
}

TypeInfo TypeInfo::getLowestCommonAncestor(const std::vector<TypeInfo> &types)
{
    if (types.empty())
        return TypeInfo(Kind::Unknown);
    if (types.size() == 1)
        return types[0];

    // Casos borde prioritarios.
    bool hasUnknown = false;
    bool hasVoid = false;
    bool allVoid = true;
    for (const auto &type : types)
    {
        hasUnknown = hasUnknown || (type.getKind() == Kind::Unknown);
        hasVoid = hasVoid || (type.getKind() == Kind::Void);
        allVoid = allVoid && (type.getKind() == Kind::Void);
    }
    if (hasUnknown)
        return TypeInfo(Kind::Unknown);
    if (hasVoid)
        return allVoid ? TypeInfo(Kind::Void) : TypeInfo(Kind::Unknown);

    auto lcaTwo = [](const TypeInfo &a, const TypeInfo &b) -> TypeInfo
    {
        // Igualdad exacta
        if (a.getKind() == b.getKind() && a.getTypeName() == b.getTypeName())
            return a;

        // Por conformidad directa, el supertipo más cercano entre ambos.
        if (a.conformsTo(b))
            return b;
        if (b.conformsTo(a))
            return a;

        // Null combinado con objeto => objeto.
        if (a.getKind() == Kind::Null && b.getKind() == Kind::Object)
            return b;
        if (b.getKind() == Kind::Null && a.getKind() == Kind::Object)
            return a;

        // Entre objetos nominales, usar jerarquía.
        if (a.getKind() == Kind::Object && b.getKind() == Kind::Object)
        {
            const std::string leftName = a.getTypeName().empty() ? "Object" : a.getTypeName();
            const std::string rightName = b.getTypeName().empty() ? "Object" : b.getTypeName();
            return TypeInfo(Kind::Object, findLowestCommonAncestor(leftName, rightName));
        }

        // Entre tipos incompatibles, el único ancestro común seguro es Object.
        return TypeInfo(Kind::Object);
    };

    TypeInfo result = types[0];
    for (size_t i = 1; i < types.size(); ++i)
    {
        result = lcaTwo(result, types[i]);
    }

    return result;
}

std::string TypeInfo::findLowestCommonAncestor(const std::string &type1, const std::string &type2)
{
    if (!symbolTable_)
        return "Object";

    if (type1 == type2)
        return type1;

    auto getParentNameOrObject = [](const TypeDecl* decl) -> std::string {
        if (!decl)
            return "Object";
        if (decl->parent_name.has_value())
            return decl->parent_name->lexeme;
        return "Object";
    };

    // Obtener la cadena de ancestros de type1 (incluyendo el propio tipo1).
    std::set<std::string> type1Ancestors;
    std::string currentName = type1;
    while (currentName != "Object")
    {
        type1Ancestors.insert(currentName);
        auto currentDecl = symbolTable_->getTypeDeclaration(currentName);
        if (!currentDecl)
            break;
        currentName = getParentNameOrObject(currentDecl);
    }
    type1Ancestors.insert("Object");

    // Recorrer type2 desde sí mismo hacia arriba y devolver el primer match.
    currentName = type2;
    while (currentName != "Object")
    {
        if (type1Ancestors.find(currentName) != type1Ancestors.end())
        {
            return currentName;
        }
        auto currentDecl = symbolTable_->getTypeDeclaration(currentName);
        if (!currentDecl)
            break;
        currentName = getParentNameOrObject(currentDecl);
    }

    return "Object";
}