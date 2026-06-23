#pragma once

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "../Parser/ast/cst_to_ast.hpp"
#include "../SymbolTable/symbol_table.hpp"
#include "diagnostic.hpp"

namespace hulk {

struct CompileOptions {
    bool all_errors = true;
};

enum class CompilePhase {
    Ok,
    Lexical,
    Syntactic,
    Semantic
};

struct CompileDiagnostic {
    CompilePhase phase = CompilePhase::Ok;
    int exit_code = 0;
    std::vector<Diagnostic> items;
    std::vector<std::string> lines;
};

struct CompiledProgram {
    CompileDiagnostic diagnostic;
    std::unique_ptr<parser::Program> program;
    SymbolTable symbol_table;
};

// Lex -> parse -> semantic. Por defecto acumula errores de todas las fases;
// con all_errors=false se detiene en la primera fase que falla.
CompileDiagnostic compile_source(const std::string& source,
                                 const std::string& grammar_path = "Parser/grammar/grammar.ll1");

// Igual que compile_source pero conserva el AST en éxito (para Fase 4 / codegen).
CompiledProgram compile_program(const std::string& source,
                                const std::string& grammar_path = "Parser/grammar/grammar.ll1",
                                CompileOptions options = {});

// Solo interpreta un programa ya validado semanticamente.
int run_interpreted(const parser::Program& program, std::ostream& out, std::ostream& err);

// Pipeline completo para el binario ./output embebido.
int run_embedded_program(const std::string& source, std::ostream& out, std::ostream& err);

}  // namespace hulk
