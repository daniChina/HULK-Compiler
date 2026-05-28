#include "decl_collector.hpp"

namespace {

std::string parentTypeName(const parser::TypeDecl &typeDecl)
{
    if (typeDecl.parent_name.has_value())
    {
        return typeDecl.parent_name->lexeme;
    }
    return "Object";
}

std::vector<TypeInfo> unknownParamTypes(const std::vector<std::pair<parser::Token, std::optional<parser::Token>>> &params)
{
    std::vector<TypeInfo> types;
    types.reserve(params.size());
    for (size_t i = 0; i < params.size(); ++i)
    {
        types.push_back(TypeInfo(TypeInfo::Kind::Unknown));
    }
    return types;
}

} // namespace

DeclCollectorResult collectTopLevelDeclarations(const parser::Program &program, SymbolTable &table)
{
    DeclCollectorResult result;

    for (const auto &stmt : program.stmts)
    {
        if (auto *functionDecl = dynamic_cast<parser::FunctionDecl *>(stmt.get()))
        {
            const auto paramTypes = unknownParamTypes(functionDecl->params);
            if (table.declareFunction(functionDecl->name.lexeme, paramTypes, TypeInfo(TypeInfo::Kind::Unknown)))
            {
                ++result.functions_registered;
            }
            continue;
        }

        if (auto *typeDecl = dynamic_cast<parser::TypeDecl *>(stmt.get()))
        {
            const std::string base = parentTypeName(*typeDecl);
            if (table.declareType(typeDecl->name.lexeme, base))
            {
                table.storeTypeDeclaration(typeDecl->name.lexeme, typeDecl);
                ++result.types_registered;
            }
            continue;
        }

        ++result.skipped_stmts;
    }

    return result;
}
