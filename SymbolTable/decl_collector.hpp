#pragma once

#include "../Parser/ast/expr.hpp"
#include "symbol_table.hpp"

/**
 * Recorre declaraciones de primer nivel en un Program (sin analizar cuerpos).
 * Usado en Fase 1 (C3) para validar que SymbolTable puede registrar lo que el AST expone.
 */
struct DeclCollectorResult
{
    int functions_registered = 0;
    int types_registered = 0;
    int skipped_stmts = 0;
};

DeclCollectorResult collectTopLevelDeclarations(const parser::Program &program, SymbolTable &table);
