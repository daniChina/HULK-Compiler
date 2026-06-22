#include "ll1_parser.hpp"

#include <stdexcept>
#include <utility>

#include "../generator/first_follow.hpp"

namespace parser {
namespace {

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
    return lookahead == "FUNCTION" || lookahead == "TYPE";
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
    if (lookahead == "TYPE") return "type";
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
               "' solo puede declararse como sentencia a nivel global, "
               "no dentro de una expresion";
    }

    return {};
}

bool is_operand_primary_lookahead(const std::string& lookahead) {
    return lookahead == "NUMBER_LITERAL" || lookahead == "STRING_LITERAL" ||
           lookahead == "NULL_LITERAL" || lookahead == "TRUE" || lookahead == "FALSE" ||
           lookahead == "LPAREN" || lookahead == "IDENTIFIER" || lookahead == "NEW" ||
           lookahead == "SELF";
}

bool is_repeated_infix_non_terminal(const std::string& non_terminal) {
    return non_terminal == "MulExpr" || non_terminal == "AddExpr" ||
           non_terminal == "ConcatExpr" || non_terminal == "CmpExpr" ||
           non_terminal == "PowerExpr" || non_terminal == "MulExprTail" ||
           non_terminal == "AddExprTail" || non_terminal == "ConcatExprTail" ||
           non_terminal == "PowerExprTail";
}

std::string repeated_infix_operator_message(const std::string& lookahead) {
    if (lookahead == "PLUS") {
        return "operador '+' inesperado: falta un operando o hay un '+' duplicado "
               "(ej. escriba 4 + 2, no 4++2)";
    }
    if (lookahead == "MINUS") {
        return "operador '-' inesperado: falta un operando o hay un '-' duplicado";
    }
    if (lookahead == "STAR") {
        return "operador '*' inesperado: falta un operando o hay '*' duplicado";
    }
    if (lookahead == "SLASH" || lookahead == "PERCENT") {
        return "operador inesperado: falta un operando o el operador esta duplicado";
    }
    if (lookahead == "CARET") {
        return "operador '^' inesperado: falta un operando o hay '^' duplicado";
    }
    if (lookahead == "CONCAT" || lookahead == "CONCAT_WS") {
        return "operador '@' inesperado: falta un operando o hay '@' duplicado "
               "(ej. escriba 'a @ b', no 'a @@ b')";
    }
    if (lookahead == "AND") {
        return "operador '&&' inesperado: falta un operando o hay '&&' duplicado";
    }
    if (lookahead == "OR") {
        return "operador '||' inesperado: falta un operando o hay '||' duplicado";
    }
    return {};
}

const char* terminal_display_name(const std::string& terminal) {
    if (terminal == "LPAREN") return "'('";
    if (terminal == "RPAREN") return "')'";
    if (terminal == "SEMICOLON") return "';'";
    if (terminal == "LBRACE") return "'{'";
    if (terminal == "RBRACE") return "'}'";
    if (terminal == "COMMA") return "','";
    if (terminal == "DOT") return "'.'";
    if (terminal == "ASSIGN") return "':='";
    if (terminal == "EQUAL") return "'='";
    if (terminal == "ARROW") return "'=>'";
    if (terminal == "COLON") return "':'";
    if (terminal == "EOF_TOKEN") return "fin de archivo";
    if (terminal == "IDENTIFIER") return "un identificador";
    if (terminal == "NUMBER_LITERAL") return "un numero";
    if (terminal == "STRING_LITERAL") return "una cadena";
    if (terminal == "TRUE") return "'true'";
    if (terminal == "FALSE") return "'false'";
    return nullptr;
}

std::string missing_type_close_message() {
    return "falta '}' para cerrar la declaracion de tipo; revise tambien ';' y ')' "
           "en atributos o metodos";
}

std::string block_context_message(
    const std::string& non_terminal,
    const Token& previous_token) {
    if (non_terminal == "Expr") {
        return "Un bloque '{ ... }' no es una expresion; solo puede usarse como cuerpo de "
               "let, if, while, with, function, metodo o como sentencia de programa";
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

std::string missing_semicolon_eof_message() {
    return "se esperaba ';' al final de la sentencia";
}

std::string missing_semicolon_before_rbrace_message() {
    return "se esperaba ';' antes de '}'";
}

std::string missing_rparen_message() {
    return "falta ')' para cerrar la expresion o la llamada a funcion";
}

std::string missing_rparen_before_rbrace_message() {
    return "falta ')' antes de '}'";
}

std::string missing_case_branch_semicolon_message() {
    return "se esperaba ';' entre ramas del case";
}

std::string invalid_equal_assignment_message(const Token& previous_token) {
    if (previous_token.type == TokenType::IDENTIFIER) {
        return std::string("no se puede usar '=' con '") + previous_token.lexeme +
               "' aqui; use 'let " + previous_token.lexeme + " = ... in ...' "
               "o '" + previous_token.lexeme + " := ...' si la variable ya existe";
    }
    return "no se puede asignar con '=' aqui; use ':=' para reasignar o 'let' para declarar";
}

std::string unexpected_semicolon_in_arglist_message() {
    return "';' inesperado dentro de la llamada a funcion; falta ')' para cerrar los argumentos";
}

std::string arg_list_tail_message(const std::string& lookahead) {
    if (lookahead == "SEMICOLON") {
        return unexpected_semicolon_in_arglist_message();
    }
    return {};
}

std::string postfix_tail_message(
    const std::string& lookahead,
    const TokenStream& tokens) {
    const int open_parens = tokens.unclosed_lparen_count();
    const int open_braces = tokens.unclosed_lbrace_count();

    if (lookahead == "EOF_TOKEN") {
        if (open_parens > 0) {
            return missing_rparen_message();
        }
        if (open_braces > 0 && tokens.has_scanned_token(TokenType::TYPE)) {
            return missing_type_close_message();
        }
        return missing_semicolon_eof_message();
    }

    if (lookahead == "RBRACE") {
        if (open_parens > 0) {
            return missing_rparen_before_rbrace_message();
        }
        return missing_semicolon_before_rbrace_message();
    }

    if (lookahead == "IDENTIFIER" && tokens.has_scanned_token(TokenType::CASE)) {
        return missing_case_branch_semicolon_message();
    }

    return {};
}

std::string postfix_tail_operand_message(
    const std::string& lookahead,
    const Token& previous_token) {
    if (previous_token.type != TokenType::IDENTIFIER) {
        return {};
    }
    if (previous_token.lexeme == "class") {
        return "HULK usa 'type' para declarar clases, no 'class'; escriba 'type Nombre { ... }'";
    }
    if (!is_operand_primary_lookahead(lookahead)) {
        return {};
    }
    return std::string("la llamada a '") + previous_token.lexeme +
           "' requiere parentesis: use " + previous_token.lexeme + "(...)";
}

std::string let_binding_separator_message(const TokenStream& tokens) {
    if (!tokens.has_scanned_token(TokenType::LET) ||
        tokens.has_scanned_token(TokenType::CASE) ||
        tokens.has_scanned_token(TokenType::IN)) {
        return {};
    }
    return "se esperaba ',' entre bindings o 'in' antes del cuerpo del let";
}

std::string binding_tail_semicolon_message() {
    return "en un let los bindings se separan con ',' y el cuerpo sigue a 'in'; "
           "no use ';' entre bindings (ej. let a = 1 in let b = 2 in { ... })";
}

std::string for_range_message() {
    return "HULK no admite rangos con '..'; escriba una expresion o coleccion valida "
           "despues de 'in'";
}

std::string let_binding_messages(
    const std::string& non_terminal,
    const std::string& lookahead,
    const TokenStream& tokens) {
    if (non_terminal == "BindingList" && lookahead == "EQUAL") {
        return "se esperaba un nombre de variable despues de 'let'";
    }
    if (non_terminal == "Expr" && lookahead == "IN" &&
        tokens.has_scanned_token(TokenType::LET)) {
        return "se esperaba una expresion despues de '=' en el binding de 'let'";
    }
    if (non_terminal == "Binding" && lookahead == "IN") {
        return "se esperaba otro binding o 'in' despues de la coma en el 'let'";
    }
    if (non_terminal == "Expr" && (lookahead == "CONCAT" || lookahead == "CONCAT_WS") &&
        tokens.has_scanned_token(TokenType::LET) && tokens.has_scanned_token(TokenType::EQUAL) &&
        !tokens.has_scanned_token(TokenType::IN)) {
        return "se esperaba una expresion; el operador '@' requiere un operando a la "
               "izquierda (ej. 'a @ b')";
    }
    return {};
}

std::string if_while_body_messages(
    const std::string& non_terminal,
    const std::string& lookahead,
    const TokenStream& tokens) {
    if (non_terminal == "IfBody") {
        if (lookahead == "ELSE") {
            return "se esperaba el cuerpo del 'if' (expresion o bloque) antes de 'else'";
        }
        if (lookahead == "SEMICOLON") {
            return "se esperaba el cuerpo del 'else' antes de ';'";
        }
    }
    if (non_terminal == "WhileBody") {
        if (lookahead == "ELSE") {
            return "se esperaba el cuerpo del 'while' antes de 'else'";
        }
        if (lookahead == "SEMICOLON") {
            return "se esperaba el cuerpo del 'else' del 'while' antes de ';'";
        }
    }
    if (non_terminal == "PostfixTail" && lookahead == "IDENTIFIER" &&
        tokens.has_scanned_token(TokenType::IF) &&
        tokens.current().lexeme == "elseif") {
        return "se esperaba 'elif' o 'else'; HULK no admite la palabra clave 'elseif'";
    }
    return {};
}

std::string stmt_context_messages(
    const std::string& non_terminal,
    const std::string& lookahead,
    const Token& previous_token) {
    if (non_terminal == "Stmt" && lookahead == "SEMICOLON" &&
        previous_token.type == TokenType::RBRACE) {
        return "un bloque usado como sentencia no lleva ';' adicional despues de '}'";
    }
    if (non_terminal == "Stmt" && lookahead == "RBRACE") {
        return "sentencia incompleta dentro del bloque; se esperaba ';' antes de '}'";
    }
    return {};
}

std::string for_header_messages(const std::string& non_terminal,
                                const std::string& lookahead,
                                const TokenStream& tokens) {
    if (non_terminal == "ForTypeOpt" && lookahead == "RPAREN") {
        return "se esperaba 'in' despues del nombre de la variable en el 'for'";
    }
    if (non_terminal == "Expr" && lookahead == "RPAREN" &&
        tokens.has_scanned_token(TokenType::FOR) &&
        tokens.has_scanned_token(TokenType::IN)) {
        return "se esperaba una expresion (coleccion o iterable) despues de 'in' en el 'for'";
    }
    return {};
}

std::string function_body_message(const std::string& non_terminal,
                                  const std::string& lookahead,
                                  const TokenStream& tokens) {
    if (non_terminal == "Expr" && lookahead == "EOF_TOKEN" &&
        tokens.has_scanned_token(TokenType::FUNCTION) &&
        tokens.has_scanned_token(TokenType::ARROW)) {
        return "se esperaba una expresion despues de '=>' en la declaracion de funcion";
    }
    return {};
}

std::string assign_messages(const std::string& non_terminal,
                            const std::string& lookahead,
                            const Token& previous_token) {
    if (non_terminal != "AssignExpr") {
        return {};
    }
    if (lookahead == "SEMICOLON" && previous_token.type == TokenType::ASSIGN) {
        return "se esperaba una expresion despues de ':='";
    }
    if (lookahead == "EQUAL" && previous_token.type == TokenType::ASSIGN) {
        return "se esperaba una expresion despues de ':='; use '=' solo en bindings de "
               "'let', no en reasignacion";
    }
    return {};
}

std::string type_decl_messages(const std::string& non_terminal,
                               const std::string& lookahead,
                               const TokenStream& tokens) {
    if (non_terminal == "ClassAttrListHead" && lookahead == "IDENTIFIER") {
        return "se esperaba ':' despues del nombre del atributo (ej. 'x: Number = 0')";
    }
    if (non_terminal == "Expr" && lookahead == "SEMICOLON" &&
        tokens.has_scanned_token(TokenType::TYPE) &&
        tokens.has_scanned_token(TokenType::EQUAL)) {
        return "se esperaba una expresion despues de '=' en el atributo del 'type'";
    }
    if (non_terminal == "PostfixTail" && lookahead == "COLON" &&
        tokens.has_scanned_token(TokenType::TYPE)) {
        return "los atributos deben declararse antes que los metodos en un 'type'";
    }
    return {};
}

std::string arg_list_messages(const std::string& non_terminal,
                              const std::string& lookahead) {
    if (non_terminal == "ArgListOpt" && lookahead == "COMMA") {
        return "se esperaba una expresion antes de ',' en la lista de argumentos";
    }
    return {};
}

std::string build_terminal_mismatch_message(
    const std::string& expected,
    const std::string& actual,
    const Token& previous_token,
    const TokenStream& tokens) {
    if (expected == "SEMICOLON" && actual == "EOF_TOKEN") {
        return missing_semicolon_eof_message();
    }
    if (expected == "SEMICOLON" && actual == "RBRACE") {
        return missing_semicolon_before_rbrace_message();
    }
    if (expected == "LPAREN" && actual == "TRUE") {
        if (tokens.has_scanned_token(TokenType::IF)) {
            return "se esperaba '(' tras 'if' para la condicion";
        }
        if (tokens.has_scanned_token(TokenType::WHILE)) {
            return "se esperaba '(' tras 'while' para la condicion";
        }
        if (tokens.has_scanned_token(TokenType::UNLESS)) {
            return "se esperaba '(' tras 'unless' para la condicion";
        }
    }
    if (expected == "LPAREN" && actual == "IDENTIFIER" &&
        tokens.has_scanned_token(TokenType::FUNCTION)) {
        return "se esperaba '(' con la lista de parametros de la funcion";
    }
    if (expected == "IDENTIFIER" && actual == "DOT" &&
        tokens.has_scanned_token(TokenType::FOR)) {
        return for_range_message();
    }
    if (expected == "IDENTIFIER" && actual == "EQUAL" &&
        tokens.has_scanned_token(TokenType::LET) &&
        tokens.has_scanned_token(TokenType::COLON)) {
        return "se esperaba el nombre del tipo despues de ':' en el binding de 'let'";
    }
    if (expected == "LPAREN" && actual == "COLON" &&
        tokens.has_scanned_token(TokenType::TYPE)) {
        return "los atributos deben declararse antes que los metodos en un 'type'";
    }
    if (expected == "EQUAL" && actual == "SEMICOLON" &&
        tokens.has_scanned_token(TokenType::TYPE)) {
        return "todo atributo debe tener valor inicial: 'nombre: Tipo = expresion;'";
    }

    const char* expected_display = terminal_display_name(expected);
    const char* actual_display = terminal_display_name(actual);
    if (expected_display != nullptr && actual_display != nullptr) {
        return std::string("se esperaba ") + expected_display + " pero se encontro " +
               actual_display;
    }
    if (expected_display != nullptr) {
        return std::string("se esperaba ") + expected_display + " pero se encontro " + actual;
    }
    return "Se esperaba el terminal " + expected + " pero se encontro " + actual;
}

std::string build_missing_production_message(
    const std::string& non_terminal,
    const std::string& lookahead,
    const Token& previous_token,
    const TokenStream& tokens) {
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

    if (non_terminal == "PostfixTail") {
        if (lookahead == "EQUAL") {
            return invalid_equal_assignment_message(previous_token);
        }
        const std::string operand_message =
            postfix_tail_operand_message(lookahead, previous_token);
        if (!operand_message.empty()) {
            return operand_message;
        }
        if (lookahead == "IDENTIFIER") {
            const std::string let_message = let_binding_separator_message(tokens);
            if (!let_message.empty()) {
                return let_message;
            }
        }
        const std::string postfix_message = postfix_tail_message(lookahead, tokens);
        if (!postfix_message.empty()) {
            return postfix_message;
        }
    }

    if (non_terminal == "BindingTail" && lookahead == "SEMICOLON") {
        return binding_tail_semicolon_message();
    }

    if (is_repeated_infix_non_terminal(non_terminal)) {
        const std::string repeated_message = repeated_infix_operator_message(lookahead);
        if (!repeated_message.empty()) {
            return repeated_message;
        }
    }

    if (non_terminal == "ArgListTail") {
        const std::string arg_message = arg_list_tail_message(lookahead);
        if (!arg_message.empty()) {
            return arg_message;
        }
    }

    const std::string let_message = let_binding_messages(non_terminal, lookahead, tokens);
    if (!let_message.empty()) {
        return let_message;
    }

    const std::string if_while_message =
        if_while_body_messages(non_terminal, lookahead, tokens);
    if (!if_while_message.empty()) {
        return if_while_message;
    }

    const std::string stmt_message =
        stmt_context_messages(non_terminal, lookahead, previous_token);
    if (!stmt_message.empty()) {
        return stmt_message;
    }

    const std::string for_message = for_header_messages(non_terminal, lookahead, tokens);
    if (!for_message.empty()) {
        return for_message;
    }

    const std::string function_message =
        function_body_message(non_terminal, lookahead, tokens);
    if (!function_message.empty()) {
        return function_message;
    }

    const std::string assign_message =
        assign_messages(non_terminal, lookahead, previous_token);
    if (!assign_message.empty()) {
        return assign_message;
    }

    const std::string type_message = type_decl_messages(non_terminal, lookahead, tokens);
    if (!type_message.empty()) {
        return type_message;
    }

    const std::string arg_list_message = arg_list_messages(non_terminal, lookahead);
    if (!arg_list_message.empty()) {
        return arg_list_message;
    }

    return "No hay produccion LL(1) para " + non_terminal + " con lookahead " + lookahead;
}

}  // namespace

Ll1Parser::Ll1Parser(
    TokenList tokens,
    const generator::Grammar& grammar,
    const generator::Ll1Table& table,
    const std::set<std::string>* stmt_first)
    : tokens_(std::move(tokens)),
      grammar_(grammar),
      table_(table),
      stmt_first_override_(stmt_first) {}

void Ll1Parser::execute_stack(std::vector<StackItem>& stack, Ll1ParseResult& result) {
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

        if (production.is_epsilon()) {
            current.node->add_child(std::make_unique<CstNode>(generator::kEpsilonSymbol));
            continue;
        }

        std::vector<StackItem> children_to_push;

        for (const auto& symbol : production.rhs) {
            CstNode* child =
                current.node->add_child(std::make_unique<CstNode>(symbol));
            children_to_push.push_back(StackItem{symbol, child});
        }

        for (auto it = children_to_push.rbegin(); it != children_to_push.rend(); ++it) {
            stack.push_back(*it);
        }
    }
}

Ll1ParseResult Ll1Parser::parse() {
    Ll1ParseResult result;
    result.cst_root = std::make_unique<CstNode>(grammar_.start_symbol);

    std::vector<StackItem> stack;
    stack.push_back(StackItem{grammar_.start_symbol, result.cst_root.get()});

    execute_stack(stack, result);

    if (!tokens_.at_end()) {
        throw ParseError(tokens_.current(), "Quedaron tokens sin consumir al final del parseo LL(1)");
    }

    return result;
}

Ll1ParseResult Ll1Parser::parse_with_recovery() {
    Ll1ParseResult result;
    result.cst_root = std::make_unique<CstNode>(grammar_.start_symbol);
    auto* program = result.cst_root.get();

    auto stmt_list_head = std::make_unique<CstNode>("StmtList");
    stmt_list_head->add_child(std::make_unique<CstNode>(generator::kEpsilonSymbol));
    CstNode* tail_stmt_list = stmt_list_head.get();
    program->add_child(std::move(stmt_list_head));

    while (!tokens_.at_end() && current_terminal() != "EOF_TOKEN") {
        auto stmt_node = std::make_unique<CstNode>("Stmt");
        std::vector<StackItem> stack;
        stack.push_back(StackItem{"Stmt", stmt_node.get()});

        try {
            execute_stack(stack, result);
            append_stmt_to_list(tail_stmt_list, std::move(stmt_node));
        } catch (const ParseError& error) {
            const auto& token = error.found();
            result.errors.push_back(
                ParseDiagnostic{token.line, token.col, error.user_message()});
            result.recovered = true;
            synchronize_after_stmt_error();
        }
    }

    if (current_terminal() == "EOF_TOKEN") {
        match_terminal("EOF_TOKEN");
    }

    return result;
}

CstNodePtr Ll1Parser::parse_stmt() {
    auto stmt_node = std::make_unique<CstNode>("Stmt");
    std::vector<StackItem> stack;
    stack.push_back(StackItem{"Stmt", stmt_node.get()});

    Ll1ParseResult scratch;
    execute_stack(stack, scratch);
    return stmt_node;
}

void Ll1Parser::append_stmt_to_list(CstNode*& tail_stmt_list, CstNodePtr stmt) {
    tail_stmt_list->children.clear();
    tail_stmt_list->add_child(std::move(stmt));
    auto next_list = std::make_unique<CstNode>("StmtList");
    next_list->add_child(std::make_unique<CstNode>(generator::kEpsilonSymbol));
    CstNode* next_ptr = next_list.get();
    tail_stmt_list->add_child(std::move(next_list));
    tail_stmt_list = next_ptr;
}

const std::set<std::string>& Ll1Parser::stmt_first_set() const {
    if (stmt_first_override_ != nullptr) {
        return *stmt_first_override_;
    }
    if (stmt_first_cached_.empty()) {
        const auto first_follow = generator::compute_first_follow(grammar_);
        stmt_first_cached_ = first_follow.first_sets.at("Stmt");
    }
    return stmt_first_cached_;
}

void Ll1Parser::synchronize_after_stmt_error() {
    if (tokens_.at_end() || current_terminal() == "EOF_TOKEN") {
        return;
    }

    const int error_line =
        tokens_.previous().line > 0 ? tokens_.previous().line : tokens_.current().line;
    tokens_.advance();

    while (!tokens_.at_end() && current_terminal() != "EOF_TOKEN") {
        if (tokens_.current().line != error_line) {
            break;
        }
        if (current_terminal() == "SEMICOLON") {
            tokens_.advance();
            break;
        }
        tokens_.advance();
    }

    const auto& sync = stmt_first_set();
    while (!tokens_.at_end() && current_terminal() != "EOF_TOKEN") {
        if (sync.count(current_terminal()) > 0) {
            break;
        }
        if (current_terminal() == "RBRACE" || current_terminal() == "SEMICOLON") {
            tokens_.advance();
            continue;
        }
        tokens_.advance();
    }
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
            build_terminal_mismatch_message(expected, actual, tokens_.previous(), tokens_));
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
            build_missing_production_message(non_terminal, lookahead, tokens_.previous(), tokens_));
    }

    return entry->second;
}

}  // namespace parser
