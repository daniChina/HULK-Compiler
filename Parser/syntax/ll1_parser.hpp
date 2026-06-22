#pragma once

#include <set>
#include <string>
#include <vector>

#include "../ast/cst_nodes.hpp"
#include "../core/parse_error.hpp"
#include "../core/token_stream.hpp"
#include "../generator/ll1_table.hpp"

namespace parser {

struct ParseDiagnostic {
    int line = 0;
    int col = 0;
    std::string message;
};

enum class ParseMode {
    Strict,
    Recovery
};

// Guarda una traza simple de las producciones aplicadas durante el parseo.
struct Ll1ParseResult {
    std::vector<generator::Production> derivation;
    CstNodePtr cst_root;
    std::vector<ParseDiagnostic> errors;
    bool recovered = false;
};

class Ll1Parser {
public:
    // El parser LL(1) necesita los tokens, la gramatica y la tabla ya construida.
    // stmt_first opcional: FIRST(Stmt) para sincronizacion en modo Recovery.
    Ll1Parser(
        TokenList tokens,
        const generator::Grammar& grammar,
        const generator::Ll1Table& table,
        const std::set<std::string>* stmt_first = nullptr);

    // Ejecuta el parseo predictivo completo a partir del simbolo inicial (Strict).
    Ll1ParseResult parse();

    // Parsea el programa con recuperacion de errores a nivel de sentencia.
    Ll1ParseResult parse_with_recovery();

    // Parsea una sola sentencia (Strict; lanza ParseError si falla).
    CstNodePtr parse_stmt();

private:
    struct StackItem {
        std::string symbol;
        CstNode* node = nullptr;
    };

    // Convierte el token actual al nombre terminal usado en grammar.ll1.
    std::string current_terminal() const;

    // Dice si un simbolo es terminal en la gramatica actual.
    bool is_terminal(const std::string& symbol) const;

    // Dice si un simbolo es no terminal en la gramatica actual.
    bool is_non_terminal(const std::string& symbol) const;

    // Consume un terminal esperado o lanza ParseError si no coincide.
    void match_terminal(const std::string& expected);

    // Expande un no terminal usando la tabla LL(1) y el lookahead actual.
    const generator::Production& select_production(const std::string& non_terminal) const;

    void execute_stack(std::vector<StackItem>& stack, Ll1ParseResult& result);

    // Tras un error de sentencia, avanza hasta FIRST(Stmt) o EOF.
    void synchronize_after_stmt_error();

    // Agrega una sentencia parseada al final de la cadena StmtList.
    void append_stmt_to_list(CstNode*& tail_stmt_list, CstNodePtr stmt);

    const std::set<std::string>& stmt_first_set() const;

    TokenStream tokens_;
    const generator::Grammar& grammar_;
    const generator::Ll1Table& table_;
    const std::set<std::string>* stmt_first_override_ = nullptr;
    mutable std::set<std::string> stmt_first_cached_;
};

}  // namespace parser
