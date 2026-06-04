#pragma once

#include "../Parser/ast/expr.hpp"
#include "symbol_table.hpp"

/**
 * Recorre declaraciones de primer nivel en un Program (sin analizar cuerpos).
 * Solo para `hulk.exe --symbols` (Fase 1): registra todas las funciones del archivo de una vez.
 * No usar antes de `--semantic`: Phase2Analyzer usa su propia SymbolTable y orden textual (R4).
 * Builtins (PI, print, …) vienen solo del constructor de SymbolTable.
 */
struct DeclCollectorResult
{
    int functions_registered = 0;
    int types_registered = 0;
    int skipped_stmts = 0;
};

DeclCollectorResult collectTopLevelDeclarations(const parser::Program &program, SymbolTable &table);
