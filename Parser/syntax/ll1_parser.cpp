#include "ll1_parser.hpp"

#include <stdexcept>
#include <utility>

namespace parser {
namespace {

// Item de pila: el simbolo a procesar y el nodo del CST asociado a ese simbolo.
struct StackItem {
    std::string symbol;
    CstNode* node = nullptr;
};

bool is_operand_non_terminal(const std::string& non_terminal) {
    return non_terminal == "Primary" || non_terminal == "PostfixExpr" ||
           non_terminal == "UnaryExpr" || non_terminal == "PowerExpr" ||
           non_terminal == "MulExpr";
}

bool is_top_level_expr_lookahead(const std::string& lookahead) {
    return lookahead == "LET" || lookahead == "IF" || lookahead == "WHILE" ||
           lookahead == "FOR" || lookahead == "WITH" || lookahead == "CASE" ||
           lookahead == "UNLESS" || lookahead == "REPEAT" || lookahead == "LOOP";
}

bool is_program_stmt_only_lookahead(const std::string& lookahead) {
    return lookahead == "FUNCTION" || lookahead == "CLASS";
}

const char* keyword_for_top_level_expr(const std::string& lookahead) {
    if (lookahead == "LET") return "let";
    if (lookahead == "IF") return "if";
    if (lookahead == "WHILE") return "while";
    if (lookahead == "FOR") return "for";
    if (lookahead == "WITH") return "with";
    if (lookahead == "CASE") return "case";
    if (lookahead == "UNLESS") return "unless";
    if (lookahead == "REPEAT") return "repeat";
    if (lookahead == "LOOP") return "loop";
    if (lookahead == "FUNCTION") return "function";
    if (lookahead == "CLASS") return "class";
    return nullptr;
}

const char* operand_category_after(TokenType op) {
    switch (op) {
    case TokenType::PLUS:
    case TokenType::MINUS:
        return "expresiones multiplicativas";
    case TokenType::STAR:
    case TokenType::SLASH:
    case TokenType::PERCENT:
        return "expresiones potencia";
    case TokenType::CARET:
        return "expresiones unarias";
    case TokenType::CONCAT:
    case TokenType::CONCAT_WS:
        return "expresiones aditivas";
    case TokenType::OR:
        return "expresiones de comparacion";
    case TokenType::AND:
        return "expresiones logicas de comparacion";
    default:
        return nullptr;
    }
}

const char* operator_lexeme_for(TokenType op) {
    switch (op) {
    case TokenType::PLUS: return "+";
    case TokenType::MINUS: return "-";
    case TokenType::STAR: return "*";
    case TokenType::SLASH: return "/";
    case TokenType::PERCENT: return "%";
    case TokenType::CARET: return "^";
    case TokenType::CONCAT: return "@";
    case TokenType::CONCAT_WS: return "@@";
    case TokenType::OR: return "||";
    case TokenType::AND: return "&&";
    default: return nullptr;
    }
}

std::string program_stmt_only_message(const std::string& non_terminal,
                                      const std::string& lookahead) {
    const char* keyword = keyword_for_top_level_expr(lookahead);
    if (keyword == nullptr || !is_program_stmt_only_lookahead(lookahead)) {
        return {};
    }

    if (non_terminal == "Expr" || non_terminal == "LetBody" || non_terminal == "IfBody" ||
        non_terminal == "WhileBody" || non_terminal == "WithBody" || non_terminal == "AssignExpr" ||
        non_terminal == "OrExpr" || non_terminal == "AndExpr" || non_terminal == "CmpExpr" ||
        non_terminal == "ConcatExpr" || non_terminal == "AddExpr" || non_terminal == "MulExpr" ||
        non_terminal == "PowerExpr" || non_terminal == "UnaryExpr" || non_terminal == "PostfixExpr" ||
        non_terminal == "Primary") {
        return std::string("'") + keyword +
               "' solo puede declararse como sentencia de programa (nivel global), "
               "no dentro de una expresion ni del cuerpo de un let";
    }

    return {};
}

std::string block_context_message(
    const std::string& non_terminal,
    const Token& previous_token) {
    if (non_terminal == "Expr") {
        return "Un bloque '{ ... }' no es una expresion; solo puede usarse como cuerpo de "
               "let, if, while, with o como cuerpo en llaves de una function";
    }
    if (non_terminal == "Program") {
        return "Un bloque '{ ... }' no puede ser una sentencia del programa por si solo; "
               "use let, if, while, function u otra expresion";
    }
    if (non_terminal == "ArgListOpt" || non_terminal == "ArgList") {
        return "Un bloque '{ ... }' no puede usarse como argumento de llamada";
    }

    const char* operand_category = operand_category_after(previous_token.type);
    const char* op_lexeme = operator_lexeme_for(previous_token.type);
    if (operand_category != nullptr && op_lexeme != nullptr &&
        is_operand_non_terminal(non_terminal)) {
        return std::string("El operador '") + op_lexeme + "' solo combina " + operand_category +
               "; un bloque '{ ... }' no es un operando valido aqui";
    }

    return {};
}

std::string build_missing_production_message(
    const std::string& non_terminal,
    const std::string& lookahead,
    const Token& previous_token) {
    if (lookahead == "LBRACE") {
        const std::string block_message = block_context_message(non_terminal, previous_token);
        if (!block_message.empty()) {
            return block_message;
        }
    }

    const std::string stmt_only_message = program_stmt_only_message(non_terminal, lookahead);
    if (!stmt_only_message.empty()) {
        return stmt_only_message;
    }

    const char* keyword = keyword_for_top_level_expr(lookahead);
    const char* operand_category = operand_category_after(previous_token.type);
    const char* op_lexeme = operator_lexeme_for(previous_token.type);

    if (keyword != nullptr && operand_category != nullptr && op_lexeme != nullptr &&
        is_operand_non_terminal(non_terminal) && is_top_level_expr_lookahead(lookahead)) {
        return std::string("El operador '") + op_lexeme +
               "' solo combina " + operand_category +
               " (literales, identificadores, llamadas, agrupacion con parentesis, etc.); '" +
               keyword + "' inicia una expresion de otro tipo y no puede usarse como operando aqui";
    }

    return "No hay produccion LL(1) para " + non_terminal + " con lookahead " + lookahead;
}

}  // namespace

Ll1Parser::Ll1Parser(
    TokenList tokens,
    const generator::Grammar& grammar,
    const generator::Ll1Table& table)
    : tokens_(std::move(tokens)),
      grammar_(grammar),
      table_(table) {}

Ll1ParseResult Ll1Parser::parse() {
    Ll1ParseResult result;
    result.cst_root = std::make_unique<CstNode>(grammar_.start_symbol);

    // La pila arranca con el simbolo inicial y el nodo raiz del CST.
    std::vector<StackItem> stack;
    stack.push_back(StackItem{grammar_.start_symbol, result.cst_root.get()});

    while (!stack.empty()) {
        const StackItem current = stack.back();
        stack.pop_back();
        const std::string& top = current.symbol;

        if (is_terminal(top)) {
            current.node->has_token = true;
            current.node->token = tokens_.current();
            match_terminal(top);
            continue;
        }

        if (!is_non_terminal(top)) {
            throw std::runtime_error("Simbolo desconocido en la pila LL(1): " + top);
        }

        const auto& production = select_production(top);
        result.derivation.push_back(production);

        // El epsilon se materializa como un hijo explicito para reflejar la derivacion.
        if (production.is_epsilon()) {
            current.node->add_child(std::make_unique<CstNode>(generator::kEpsilonSymbol));
            continue;
        }

        std::vector<StackItem> children_to_push;

        // Los hijos se crean en orden natural para preservar el arbol.
        for (const auto& symbol : production.rhs) {
            CstNode* child =
                current.node->add_child(std::make_unique<CstNode>(symbol));
            children_to_push.push_back(StackItem{symbol, child});
        }

        // El lado derecho se empuja al reves para que el primer simbolo quede arriba.
        for (auto it = children_to_push.rbegin(); it != children_to_push.rend(); ++it) {
            stack.push_back(*it);
        }
    }

    // Si todavia quedan tokens distintos de EOF, hubo entrada sin consumir.
    if (!tokens_.at_end()) {
        throw ParseError(tokens_.current(), "Quedaron tokens sin consumir al final del parseo LL(1)");
    }

    return result;
}

std::string Ll1Parser::current_terminal() const {
    return token_name(tokens_.current().type);
}

bool Ll1Parser::is_terminal(const std::string& symbol) const {
    return grammar_.terminals.count(symbol) == 1;
}

bool Ll1Parser::is_non_terminal(const std::string& symbol) const {
    return grammar_.non_terminals.count(symbol) == 1;
}

void Ll1Parser::match_terminal(const std::string& expected) {
    const auto actual = current_terminal();
    if (actual != expected) {
        throw ParseError(
            tokens_.current(),
            "Se esperaba el terminal " + expected + " pero se encontro " + actual);
    }

    // EOF se considera consumido logicamente al finalizar; no hace falta avanzar.
    if (expected == "EOF_TOKEN") {
        return;
    }

    tokens_.advance();
}

const generator::Production& Ll1Parser::select_production(const std::string& non_terminal) const {
    const auto row = table_.find(non_terminal);
    if (row == table_.end()) {
        throw ParseError(
            tokens_.current(),
            "No existe fila LL(1) para el no terminal " + non_terminal);
    }

    const auto lookahead = current_terminal();
    const auto entry = row->second.find(lookahead);
    if (entry == row->second.end()) {
        throw ParseError(
            tokens_.current(),
            build_missing_production_message(non_terminal, lookahead, tokens_.previous()));
    }

    return entry->second;
}

}  // namespace parser
