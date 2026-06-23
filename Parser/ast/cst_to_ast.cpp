#include "cst_to_ast.hpp"

#include <stdexcept>
#include <utility>
#include <vector>

#include "../../SemanticCheck/binding_list.hpp"
#include "../core/parse_error.hpp"
#include "../generator/production.hpp"

namespace parser {
namespace {

// Verifica la forma estructural esperada del CST antes de convertirla a AST.
void expect_symbol(const CstNode& node, const std::string& expected) {
    if (node.symbol != expected) {
        throw std::runtime_error(
            "Se esperaba nodo CST " + expected + " pero se encontro " + node.symbol);
    }
}

// Acceso corto para no repetir desreferenciaciones sobre `children`.
const CstNode& child(const CstNode& node, std::size_t index) {
    if (index >= node.children.size()) {
        throw std::runtime_error(
            "Indice de hijo fuera de rango en nodo CST " + node.symbol);
    }

    return *node.children[index];
}

bool is_epsilon_node(const CstNode& node) {
    return node.symbol == generator::kEpsilonSymbol;
}

ExprPtr build_expr(const CstNode& node);
ExprPtr build_if_expr(const CstNode& node);
ExprPtr build_while_expr(const CstNode& node);
ExprPtr build_for_expr(const CstNode& node);
ExprPtr build_for_stmt(const CstNode& node);
ExprPtr build_for_stmt_body(const CstNode& node);
std::optional<Token> build_for_type_opt(const CstNode& node);
ExprPtr build_unless_expr(const CstNode& node);
ExprPtr build_repeat_expr(const CstNode& node);
ExprPtr build_loop_while_expr(const CstNode& node);
ExprPtr build_with_expr(const CstNode& node);
ExprPtr build_case_expr(const CstNode& node);
ExprPtr build_elif_chain_opt(const CstNode& node, ExprPtr final_else);
ExprPtr build_else_opt(const CstNode& node);
ExprPtr build_let_expr(const CstNode& node);
ExprPtr build_let_body(const CstNode& node);
ExprPtr build_if_body(const CstNode& node);
ExprPtr build_while_body(const CstNode& node);
ExprPtr build_for_body(const CstNode& node);
ExprPtr build_while_else_opt(const CstNode& node);
ExprPtr build_with_body(const CstNode& node);
ExprPtr build_with_else_opt(const CstNode& node);
ExprPtr build_case_body(const CstNode& node);
using LetBindingData = std::tuple<Token, std::optional<Token>, ExprPtr>;
using CaseBranchList = std::vector<CaseBranchDef>;
void extract_bindings(const CstNode& node, std::vector<LetBindingData>& bindings);
void extract_binding(const CstNode& node, std::vector<LetBindingData>& bindings);
void extract_binding_tail(const CstNode& node, std::vector<LetBindingData>& bindings);
CaseBranchDef build_case_branch(const CstNode& node);
CaseBranchList build_case_payload(const CstNode& node);
CaseBranchList build_case_branch_list(const CstNode& node);
void append_case_branch_list_tail(const CstNode& node, CaseBranchList& branches);

ExprPtr build_assign_expr(const CstNode& node);
ExprPtr build_assign_tail(ExprPtr left, const CstNode& node);
ExprPtr build_or_expr(const CstNode& node);
ExprPtr build_or_tail(ExprPtr left, const CstNode& node);
ExprPtr build_and_expr(const CstNode& node);
ExprPtr build_and_tail(ExprPtr left, const CstNode& node);
ExprPtr build_cmp_expr(const CstNode& node);
ExprPtr build_cmp_tail(ExprPtr left, const CstNode& node);
ExprPtr build_concat_expr(const CstNode& node);
ExprPtr build_concat_tail(ExprPtr left, const CstNode& node);
ExprPtr build_add_expr(const CstNode& node);
ExprPtr build_add_tail(ExprPtr left, const CstNode& node);
ExprPtr build_mul_expr(const CstNode& node);
ExprPtr build_mul_tail(ExprPtr left, const CstNode& node);
ExprPtr build_power_expr(const CstNode& node);
ExprPtr build_power_tail(ExprPtr left, const CstNode& node);
ExprPtr build_unary_expr(const CstNode& node);
ExprPtr build_postfix_expr(const CstNode& node);
ExprPtr build_postfix_tail(ExprPtr left, const CstNode& node);
ExprPtr build_as_expr_opt(ExprPtr left, const CstNode& node);

// Esta familia paralela evita que el `as` propio de `with (...) as id`
// sea absorbido por el cast postfix general de otras expresiones.
ExprPtr build_with_source_expr(const CstNode& node);
ExprPtr build_with_source_assign_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_or_expr(const CstNode& node);
ExprPtr build_with_source_or_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_and_expr(const CstNode& node);
ExprPtr build_with_source_and_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_cmp_expr(const CstNode& node);
ExprPtr build_with_source_cmp_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_concat_expr(const CstNode& node);
ExprPtr build_with_source_concat_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_add_expr(const CstNode& node);
ExprPtr build_with_source_add_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_mul_expr(const CstNode& node);
ExprPtr build_with_source_mul_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_power_expr(const CstNode& node);
ExprPtr build_with_source_power_tail(ExprPtr left, const CstNode& node);
ExprPtr build_with_source_unary_expr(const CstNode& node);
ExprPtr build_with_source_postfix_expr(const CstNode& node);
ExprPtr build_primary(const CstNode& node);
ExprPtr build_block_expr(const CstNode& node);
void extract_block_list(const CstNode& node, std::vector<ExprPtr>& exprs);
std::vector<ExprPtr> build_arg_list_opt(const CstNode& node);
std::vector<ExprPtr> build_arg_list(const CstNode& node);
void append_arg_list_tail(const CstNode& node, std::vector<ExprPtr>& args);

void extract_stmt_list(const CstNode& node, std::vector<StmtPtr>& stmts);
StmtPtr build_stmt(const CstNode& node);
StmtPtr build_expr_stmt(const CstNode& node);
StmtPtr build_block_stmt(const CstNode& node);
ExprPtr build_block_list_head(const CstNode& node);
StmtPtr build_function_decl(const CstNode& node);

std::vector<std::pair<Token, std::optional<Token>>> build_arg_id_list_opt(const CstNode& node);
std::vector<std::pair<Token, std::optional<Token>>> build_arg_id_list(const CstNode& node);
void extract_arg_id_list_tail(const CstNode& node, std::vector<std::pair<Token, std::optional<Token>>>& args);
std::pair<Token, std::optional<Token>> build_arg_id(const CstNode& node);
std::optional<Token> build_type_annotation_opt(const CstNode& node);
std::optional<Token> build_method_return_type_opt(const CstNode& node);
ExprPtr build_function_body(const CstNode& node);
ExprPtr build_method_body(const CstNode& node);

StmtPtr build_class_decl(const CstNode& node);
Token build_type_annotation(const CstNode& node);
AttributeDef build_class_attr_from_head(const CstNode& head_node, Token name);
MethodDef build_class_method_from_head(const CstNode& head_node, Token name);
void extract_class_attr_list_head(const CstNode& head_node, Token name, std::vector<AttributeDef>& attributes, std::vector<MethodDef>& methods);
void extract_class_attr_list(const CstNode& node, std::vector<AttributeDef>& attributes, std::vector<MethodDef>& methods);
void extract_class_method_list(const CstNode& node, std::vector<MethodDef>& methods);
MethodDef build_class_method(const CstNode& node);
void extract_class_body(const CstNode& node, std::vector<AttributeDef>& attributes, std::vector<MethodDef>& methods);

StmtPtr build_stmt(const CstNode& node) {
    expect_symbol(node, "Stmt");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "ExprStmt") {
        return build_expr_stmt(first_child);
    }
    if (first_child.symbol == "FunctionDecl") {
        return build_function_decl(first_child);
    }
    if (first_child.symbol == "ClassDecl") {
        return build_class_decl(first_child);
    }
    if (first_child.symbol == "BlockStmt") {
        return build_block_stmt(first_child);
    }
    if (first_child.symbol == "ForStmt") {
        return std::make_unique<ExprStmt>(build_for_stmt(first_child));
    }
    throw std::runtime_error("Forma no esperada en Stmt");
}

StmtPtr build_block_stmt(const CstNode& node) {
    expect_symbol(node, "BlockStmt");
    return std::make_unique<ExprStmt>(build_block_expr(child(node, 0)));
}

StmtPtr build_expr_stmt(const CstNode& node) {
    expect_symbol(node, "ExprStmt");
    return std::make_unique<ExprStmt>(build_expr(child(node, 0)));
}

ExprPtr build_expr(const CstNode& node) {
    expect_symbol(node, "Expr");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "IfExpr") {
        return build_if_expr(first_child);
    }
    if (first_child.symbol == "WhileExpr") {
        return build_while_expr(first_child);
    }
    if (first_child.symbol == "WithExpr") {
        return build_with_expr(first_child);
    }
    if (first_child.symbol == "CaseExpr") {
        return build_case_expr(first_child);
    }
    if (first_child.symbol == "LetExpr") {
        return build_let_expr(first_child);
    }
    if (first_child.symbol == "AssignExpr") {
        return build_assign_expr(first_child);
    }
    if (first_child.symbol == "UnlessExpr") {
        return build_unless_expr(first_child);
    }
    if (first_child.symbol == "RepeatExpr") {
        return build_repeat_expr(first_child);
    }
    if (first_child.symbol == "LoopWhileExpr") {
        return build_loop_while_expr(first_child);
    }
    throw std::runtime_error("Forma no esperada en Expr: se esperaba IfExpr, WhileExpr, WithExpr, CaseExpr, LetExpr, AssignExpr, UnlessExpr, RepeatExpr o LoopWhileExpr");
}

std::optional<Token> build_for_type_opt(const CstNode& node) {
    expect_symbol(node, "ForTypeOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return std::nullopt;
    }
    return child(node, 1).token;
}

ExprPtr build_for_expr(const CstNode& node) {
    expect_symbol(node, "ForExpr");
    Token variable = child(node, 2).token;
    auto declared_type = build_for_type_opt(child(node, 3));
    ExprPtr iterable = build_expr(child(node, 5));
    ExprPtr body = build_for_body(child(node, 7));
    return std::make_unique<ForExpr>(std::move(variable), std::move(declared_type), std::move(iterable), std::move(body));
}

ExprPtr build_for_stmt(const CstNode& node) {
    expect_symbol(node, "ForStmt");
    Token variable = child(node, 2).token;
    auto declared_type = build_for_type_opt(child(node, 3));
    ExprPtr iterable = build_expr(child(node, 5));
    ExprPtr body = build_for_stmt_body(child(node, 7));
    return std::make_unique<ForExpr>(std::move(variable), std::move(declared_type), std::move(iterable), std::move(body));
}

ExprPtr build_for_stmt_body(const CstNode& node) {
    expect_symbol(node, "ForStmtBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

ExprPtr build_for_body(const CstNode& node) {
    expect_symbol(node, "ForBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

ExprPtr build_unless_expr(const CstNode& node) {
    expect_symbol(node, "UnlessExpr");
    ExprPtr cond = build_expr(child(node, 2));
    ExprPtr then_branch = build_expr(child(node, 4));
    ExprPtr else_branch = build_else_opt(child(node, 5));
    return std::make_unique<UnlessExpr>(std::move(cond), std::move(then_branch), std::move(else_branch));
}

ExprPtr build_repeat_expr(const CstNode& node) {
    expect_symbol(node, "RepeatExpr");
    ExprPtr count = build_expr(child(node, 2));
    ExprPtr body = build_expr(child(node, 4));
    return std::make_unique<RepeatExpr>(std::move(count), std::move(body));
}

ExprPtr build_loop_while_expr(const CstNode& node) {
    expect_symbol(node, "LoopWhileExpr");
    ExprPtr body = build_expr(child(node, 1));
    ExprPtr condition = build_expr(child(node, 4));
    return std::make_unique<LoopWhileExpr>(std::move(body), std::move(condition));
}

ExprPtr build_while_expr(const CstNode& node) {
    expect_symbol(node, "WhileExpr");
    ExprPtr cond = build_expr(child(node, 2));
    ExprPtr body = build_while_body(child(node, 4));
    ExprPtr else_branch = build_while_else_opt(child(node, 5));
    return std::make_unique<WhileExpr>(std::move(cond), std::move(body), std::move(else_branch));
}

ExprPtr build_with_expr(const CstNode& node) {
    expect_symbol(node, "WithExpr");
    ExprPtr value = build_with_source_expr(child(node, 2));
    Token alias = child(node, 4).token;
    ExprPtr body = build_with_body(child(node, 6));
    ExprPtr else_branch = build_with_else_opt(child(node, 7));
    return std::make_unique<WithExpr>(std::move(value), std::move(alias), std::move(body), std::move(else_branch));
}

ExprPtr build_case_expr(const CstNode& node) {
    expect_symbol(node, "CaseExpr");
    ExprPtr value = build_expr(child(node, 1));
    auto branches = build_case_payload(child(node, 3));
    return std::make_unique<CaseExpr>(std::move(value), std::move(branches));
}

ExprPtr build_if_expr(const CstNode& node) {
    expect_symbol(node, "IfExpr");
    ExprPtr cond = build_expr(child(node, 2));
    ExprPtr then_branch = build_if_body(child(node, 4));
    ExprPtr else_branch = build_else_opt(child(node, 6));

    return std::make_unique<IfExpr>(
        std::move(cond),
        std::move(then_branch),
        build_elif_chain_opt(child(node, 5), std::move(else_branch)));
}

ExprPtr build_else_opt(const CstNode& node) {
    expect_symbol(node, "ElseOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return nullptr;
    }
    return build_if_body(child(node, 1));
}

ExprPtr build_elif_chain_opt(const CstNode& node, ExprPtr final_else) {
    expect_symbol(node, "ElifChainOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return final_else;
    }
    ExprPtr cond = build_expr(child(node, 2));
    ExprPtr then_branch = build_if_body(child(node, 4));
    ExprPtr nested_else = build_elif_chain_opt(child(node, 5), std::move(final_else));

    return std::make_unique<IfExpr>(std::move(cond), std::move(then_branch), std::move(nested_else));
}

ExprPtr build_let_expr(const CstNode& node) {
    expect_symbol(node, "LetExpr");
    
    std::vector<LetBindingData> bindings;
    extract_bindings(child(node, 1), bindings);

    semantic::enqueueDuplicateLetBindingErrors(bindings);
    
    ExprPtr body = build_let_body(child(node, 3));
    
    for (int i = static_cast<int>(bindings.size()) - 1; i >= 0; --i) {
        body = std::make_unique<LetExpr>(
            std::move(std::get<0>(bindings[i])),
            std::move(std::get<1>(bindings[i])),
            std::move(std::get<2>(bindings[i])),
            std::move(body)
        );
    }
    
    return body;
}

ExprPtr build_let_body(const CstNode& node) {
    expect_symbol(node, "LetBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

ExprPtr build_if_body(const CstNode& node) {
    expect_symbol(node, "IfBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

ExprPtr build_while_body(const CstNode& node) {
    expect_symbol(node, "WhileBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

ExprPtr build_while_else_opt(const CstNode& node) {
    expect_symbol(node, "WhileElseOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return nullptr;
    }
    return build_while_body(child(node, 1));
}

ExprPtr build_with_body(const CstNode& node) {
    expect_symbol(node, "WithBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

ExprPtr build_with_else_opt(const CstNode& node) {
    expect_symbol(node, "WithElseOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return nullptr;
    }
    return build_with_body(child(node, 1));
}

ExprPtr build_case_body(const CstNode& node) {
    expect_symbol(node, "CaseBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "BlockExpr") {
        return build_block_expr(first_child);
    }
    return build_expr(first_child);
}

void extract_bindings(const CstNode& node, std::vector<LetBindingData>& bindings) {
    expect_symbol(node, "BindingList");
    extract_binding(child(node, 0), bindings);
    extract_binding_tail(child(node, 1), bindings);
}

void extract_binding(const CstNode& node, std::vector<LetBindingData>& bindings) {
    expect_symbol(node, "Binding");
    Token name = child(node, 0).token;
    std::optional<Token> declared_type = build_type_annotation_opt(child(node, 1));
    ExprPtr init = build_expr(child(node, 3));
    bindings.emplace_back(std::move(name), std::move(declared_type), std::move(init));
}

void extract_binding_tail(const CstNode& node, std::vector<LetBindingData>& bindings) {
    expect_symbol(node, "BindingTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }
    extract_binding(child(node, 1), bindings);
    extract_binding_tail(child(node, 2), bindings);
}

CaseBranchDef build_case_branch(const CstNode& node) {
    expect_symbol(node, "CaseBranch");
    Token name = child(node, 0).token;
    Token type_name = child(node, 2).token;
    ExprPtr body = build_case_body(child(node, 4));
    return {std::move(name), std::move(type_name), std::move(body)};
}

CaseBranchList build_case_payload(const CstNode& node) {
    expect_symbol(node, "CasePayload");
    if (child(node, 0).symbol == "LBRACE") {
        return build_case_branch_list(child(node, 1));
    }

    CaseBranchList branches;
    branches.push_back(build_case_branch(child(node, 0)));
    return branches;
}

CaseBranchList build_case_branch_list(const CstNode& node) {
    expect_symbol(node, "CaseBranchList");
    CaseBranchList branches;
    branches.push_back(build_case_branch(child(node, 0)));
    append_case_branch_list_tail(child(node, 2), branches);
    return branches;
}

void append_case_branch_list_tail(const CstNode& node, CaseBranchList& branches) {
    expect_symbol(node, "CaseBranchListTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }

    branches.push_back(build_case_branch(child(node, 0)));
    append_case_branch_list_tail(child(node, 2), branches);
}

ExprPtr build_assign_expr(const CstNode& node) {
    expect_symbol(node, "AssignExpr");
    auto left = build_or_expr(child(node, 0));
    return build_assign_tail(std::move(left), child(node, 1));
}

ExprPtr build_assign_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "AssignTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_assign_expr(child(node, 1));
    return std::make_unique<AssignExpr>(std::move(left), std::move(op), std::move(right));
}

ExprPtr build_or_expr(const CstNode& node) {
    expect_symbol(node, "OrExpr");
    auto left = build_and_expr(child(node, 0));
    return build_or_tail(std::move(left), child(node, 1));
}

ExprPtr build_or_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "OrExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_and_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_or_tail(std::move(combined), child(node, 2));
}

ExprPtr build_and_expr(const CstNode& node) {
    expect_symbol(node, "AndExpr");
    auto left = build_cmp_expr(child(node, 0));
    return build_and_tail(std::move(left), child(node, 1));
}

ExprPtr build_and_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "AndExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_cmp_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_and_tail(std::move(combined), child(node, 2));
}

ExprPtr build_cmp_expr(const CstNode& node) {
    expect_symbol(node, "CmpExpr");
    auto left = build_concat_expr(child(node, 0));
    return build_cmp_tail(std::move(left), child(node, 1));
}

ExprPtr build_cmp_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "CmpExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    const auto& op_node = child(node, 0);
    if (op_node.token.type == TokenType::IS) {
        Token is_keyword = op_node.token;
        Token type_name = child(node, 1).token;
        auto is_expr = std::make_unique<IsExpr>(std::move(left), std::move(is_keyword), std::move(type_name));
        return build_cmp_tail(std::move(is_expr), child(node, 2));
    }

    Token op = op_node.token;
    ExprPtr right = build_concat_expr(child(node, 1));
    auto new_left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));

    return build_cmp_tail(std::move(new_left), child(node, 2));
}

ExprPtr build_concat_expr(const CstNode& node) {
    expect_symbol(node, "ConcatExpr");
    auto left = build_add_expr(child(node, 0));
    return build_concat_tail(std::move(left), child(node, 1));
}

ExprPtr build_concat_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "ConcatExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_add_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_concat_tail(std::move(combined), child(node, 2));
}

ExprPtr build_add_expr(const CstNode& node) {
    expect_symbol(node, "AddExpr");
    auto left = build_mul_expr(child(node, 0));
    return build_add_tail(std::move(left), child(node, 1));
}

ExprPtr build_add_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "AddExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_mul_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_add_tail(std::move(combined), child(node, 2));
}

ExprPtr build_mul_expr(const CstNode& node) {
    expect_symbol(node, "MulExpr");
    auto left = build_power_expr(child(node, 0));
    return build_mul_tail(std::move(left), child(node, 1));
}

ExprPtr build_mul_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "MulExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_power_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_mul_tail(std::move(combined), child(node, 2));
}

ExprPtr build_power_expr(const CstNode& node) {
    expect_symbol(node, "PowerExpr");
    auto left = build_unary_expr(child(node, 0));
    return build_power_tail(std::move(left), child(node, 1));
}

ExprPtr build_power_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "PowerExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    // Aqui se conserva la asociatividad derecha: el lado derecho vuelve a ser PowerExpr completo.
    Token op = child(node, 0).token;
    auto right = build_power_expr(child(node, 1));
    return std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
}

ExprPtr build_unary_expr(const CstNode& node) {
    expect_symbol(node, "UnaryExpr");

    if (node.children.size() == 1) {
        const auto& only_child = child(node, 0);
        if (only_child.symbol == "PostfixExpr") {
            return build_postfix_expr(only_child);
        }
        return build_primary(only_child);
    }

    Token op = child(node, 0).token;
    auto right = build_unary_expr(child(node, 1));
    return std::make_unique<UnaryExpr>(std::move(op), std::move(right));
}

ExprPtr build_as_expr_opt(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "AsExprOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token as_keyword = child(node, 0).token;
    Token type_name = child(node, 1).token;
    auto as_expr = std::make_unique<AsExpr>(std::move(left), std::move(as_keyword), std::move(type_name));
    return build_as_expr_opt(std::move(as_expr), child(node, 2));
}

ExprPtr build_postfix_expr(const CstNode& node) {
    expect_symbol(node, "PostfixExpr");
    auto left = build_primary(child(node, 0));
    left = build_postfix_tail(std::move(left), child(node, 1));
    return build_as_expr_opt(std::move(left), child(node, 2));
}

ExprPtr build_with_source_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceExpr");
    auto left = build_with_source_or_expr(child(node, 0));
    return build_with_source_assign_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_assign_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceAssignTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_expr(child(node, 1));
    return std::make_unique<AssignExpr>(std::move(left), std::move(op), std::move(right));
}

ExprPtr build_with_source_or_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceOrExpr");
    auto left = build_with_source_and_expr(child(node, 0));
    return build_with_source_or_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_or_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceOrExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_and_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_with_source_or_tail(std::move(combined), child(node, 2));
}

ExprPtr build_with_source_and_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceAndExpr");
    auto left = build_with_source_cmp_expr(child(node, 0));
    return build_with_source_and_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_and_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceAndExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_cmp_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_with_source_and_tail(std::move(combined), child(node, 2));
}

ExprPtr build_with_source_cmp_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceCmpExpr");
    auto left = build_with_source_concat_expr(child(node, 0));
    return build_with_source_cmp_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_cmp_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceCmpExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    const auto& op_node = child(node, 0);
    Token op = op_node.token;
    ExprPtr right = build_with_source_concat_expr(child(node, 1));
    auto new_left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right));
    return build_with_source_cmp_tail(std::move(new_left), child(node, 2));
}

ExprPtr build_with_source_concat_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceConcatExpr");
    auto left = build_with_source_add_expr(child(node, 0));
    return build_with_source_concat_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_concat_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceConcatExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_add_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_with_source_concat_tail(std::move(combined), child(node, 2));
}

ExprPtr build_with_source_add_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceAddExpr");
    auto left = build_with_source_mul_expr(child(node, 0));
    return build_with_source_add_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_add_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceAddExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_mul_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_with_source_add_tail(std::move(combined), child(node, 2));
}

ExprPtr build_with_source_mul_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceMulExpr");
    auto left = build_with_source_power_expr(child(node, 0));
    return build_with_source_mul_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_mul_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourceMulExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_power_expr(child(node, 1));
    auto combined = std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
    return build_with_source_mul_tail(std::move(combined), child(node, 2));
}

ExprPtr build_with_source_power_expr(const CstNode& node) {
    expect_symbol(node, "WithSourcePowerExpr");
    auto left = build_with_source_unary_expr(child(node, 0));
    return build_with_source_power_tail(std::move(left), child(node, 1));
}

ExprPtr build_with_source_power_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "WithSourcePowerExprTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_power_expr(child(node, 1));
    return std::make_unique<BinaryExpr>(std::move(left), std::move(op), std::move(right));
}

ExprPtr build_with_source_unary_expr(const CstNode& node) {
    expect_symbol(node, "WithSourceUnaryExpr");
    if (node.children.size() == 1) {
        return build_with_source_postfix_expr(child(node, 0));
    }

    Token op = child(node, 0).token;
    auto right = build_with_source_unary_expr(child(node, 1));
    return std::make_unique<UnaryExpr>(std::move(op), std::move(right));
}

ExprPtr build_with_source_postfix_expr(const CstNode& node) {
    expect_symbol(node, "WithSourcePostfixExpr");
    auto left = build_primary(child(node, 0));
    return build_postfix_tail(std::move(left), child(node, 1));
}

ExprPtr build_postfix_tail(ExprPtr left, const CstNode& node) {
    expect_symbol(node, "PostfixTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return left;
    }

    if (child(node, 0).symbol == "LPAREN") {
        Token lparen = child(node, 0).token;
        auto args = build_arg_list_opt(child(node, 1));
        if (auto* id = dynamic_cast<IdentifierExpr*>(left.get());
            id != nullptr && id->token.lexeme == "base") {
            return build_postfix_tail(
                std::make_unique<BaseCallExpr>(std::move(args)), child(node, 3));
        }
        auto call = std::make_unique<CallExpr>(std::move(left), std::move(lparen), std::move(args));
        return build_postfix_tail(std::move(call), child(node, 3));
    }

    if (child(node, 0).symbol == "DOT") {
        Token dot = child(node, 0).token;
        Token name = child(node, 1).token;
        auto access = std::make_unique<GetAttrExpr>(std::move(left), std::move(dot), std::move(name));
        return build_postfix_tail(std::move(access), child(node, 2));
    }

    throw std::runtime_error("Forma de PostfixTail no soportada en la conversion CST -> AST");
}

std::vector<ExprPtr> build_arg_list_opt(const CstNode& node) {
    expect_symbol(node, "ArgListOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return {};
    }
    return build_arg_list(child(node, 0));
}

std::vector<ExprPtr> build_arg_list(const CstNode& node) {
    expect_symbol(node, "ArgList");
    std::vector<ExprPtr> args;
    args.push_back(build_expr(child(node, 0)));
    append_arg_list_tail(child(node, 1), args);
    return args;
}

void append_arg_list_tail(const CstNode& node, std::vector<ExprPtr>& args) {
    expect_symbol(node, "ArgListTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }

    args.push_back(build_expr(child(node, 1)));
    append_arg_list_tail(child(node, 2), args);
}

ExprPtr build_primary(const CstNode& node) {
    expect_symbol(node, "Primary");

    if (node.children.size() == 1) {
        const auto& value = child(node, 0);

        if (value.symbol == "NUMBER_LITERAL") {
            return std::make_unique<NumberExpr>(value.token);
        }
        if (value.symbol == "STRING_LITERAL") {
            return std::make_unique<StringExpr>(value.token);
        }
        if (value.symbol == "NULL_LITERAL") {
            return std::make_unique<NullExpr>(value.token);
        }
        if (value.symbol == "TRUE") {
            return std::make_unique<BoolExpr>(value.token, true);
        }
        if (value.symbol == "FALSE") {
            return std::make_unique<BoolExpr>(value.token, false);
        }
        if (value.symbol == "IDENTIFIER") {
            return std::make_unique<IdentifierExpr>(value.token);
        }
        if (value.symbol == "SELF") {
            // `self` es un primario dedicado porque no debe confundirse con
            // un identificador ordinario durante fases semánticas posteriores.
            return std::make_unique<SelfExpr>(value.token);
        }
    }

    // Caso agrupado: LPAREN Expr RPAREN
    if (node.children.size() == 3 && child(node, 0).symbol == "LPAREN") {
        Token lparen = child(node, 0).token;
        auto expression = build_expr(child(node, 1));
        return std::make_unique<GroupedExpr>(std::move(lparen), std::move(expression));
    }

    // Instanciación directa: NEW IDENTIFIER LPAREN ArgListOpt RPAREN
    if (node.children.size() == 5 && child(node, 0).symbol == "NEW") {
        Token type_name = child(node, 1).token;
        auto args = build_arg_list_opt(child(node, 3));
        return std::make_unique<NewExpr>(std::move(type_name), std::move(args));
    }

    throw std::runtime_error("Forma de Primary no soportada en la conversion CST -> AST");
}

ExprPtr build_block_expr(const CstNode& node) {
    expect_symbol(node, "BlockExpr");
    std::vector<ExprPtr> exprs;
    extract_block_list(child(node, 1), exprs);
    return std::make_unique<BlockExpr>(std::move(exprs));
}

void extract_block_list(const CstNode& node, std::vector<ExprPtr>& exprs) {
    expect_symbol(node, "BlockList");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }
    exprs.push_back(build_block_list_head(child(node, 0)));
    extract_block_list(child(node, 2), exprs);
}

ExprPtr build_block_list_head(const CstNode& node) {
    expect_symbol(node, "BlockListHead");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "ForExpr") {
        return build_for_expr(first_child);
    }
    return build_expr(first_child);
}

void extract_stmt_list(const CstNode& node, std::vector<StmtPtr>& stmts) {
    expect_symbol(node, "StmtList");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }
    stmts.push_back(build_stmt(child(node, 0)));
    extract_stmt_list(child(node, 1), stmts);
}

StmtPtr build_function_decl(const CstNode& node) {
    expect_symbol(node, "FunctionDecl");
    // FUNCTION IDENTIFIER LPAREN ArgIdListOpt RPAREN TypeAnnotationOpt FunctionBody
    Token name = child(node, 1).token;
    auto params = build_arg_id_list_opt(child(node, 3));
    auto return_type = build_type_annotation_opt(child(node, 5));
    auto body = build_function_body(child(node, 6));
    return std::make_unique<FunctionDecl>(std::move(name), std::move(params), std::move(return_type), std::move(body));
}

std::vector<std::pair<Token, std::optional<Token>>> build_arg_id_list_opt(const CstNode& node) {
    expect_symbol(node, "ArgIdListOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return {};
    }
    return build_arg_id_list(child(node, 0));
}

std::vector<std::pair<Token, std::optional<Token>>> build_arg_id_list(const CstNode& node) {
    expect_symbol(node, "ArgIdList");
    std::vector<std::pair<Token, std::optional<Token>>> args;
    args.push_back(build_arg_id(child(node, 0)));
    extract_arg_id_list_tail(child(node, 1), args);
    return args;
}

void extract_arg_id_list_tail(const CstNode& node, std::vector<std::pair<Token, std::optional<Token>>>& args) {
    expect_symbol(node, "ArgIdListTail");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }
    args.push_back(build_arg_id(child(node, 1)));
    extract_arg_id_list_tail(child(node, 2), args);
}

std::pair<Token, std::optional<Token>> build_arg_id(const CstNode& node) {
    expect_symbol(node, "ArgId");
    Token name = child(node, 0).token;
    auto type = build_type_annotation_opt(child(node, 1));
    return {std::move(name), std::move(type)};
}

std::optional<Token> build_type_annotation_opt(const CstNode& node) {
    expect_symbol(node, "TypeAnnotationOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return std::nullopt;
    }
    return child(node, 1).token;
}

ExprPtr build_function_body(const CstNode& node) {
    expect_symbol(node, "FunctionBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "ARROW") {
        return build_expr(child(node, 1));
    }
    return build_block_expr(first_child);
}

std::optional<Token> build_method_return_type_opt(const CstNode& node) {
    expect_symbol(node, "MethodReturnTypeOpt");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return std::nullopt;
    }
    return build_type_annotation(child(node, 0));
}

ExprPtr build_method_body(const CstNode& node) {
    expect_symbol(node, "MethodBody");
    const auto& first_child = child(node, 0);
    if (first_child.symbol == "ARROW") {
        return build_expr(child(node, 1));
    }
    return build_block_expr(first_child);
}

StmtPtr build_class_decl(const CstNode& node) {
    expect_symbol(node, "ClassDecl");
    // TYPE IDENTIFIER ClassParamsOpt ClassInheritanceOpt LBRACE ClassBody RBRACE
    Token name = child(node, 1).token;

    // ClassParamsOpt -> LPAREN ArgIdListOpt RPAREN | ε
    std::vector<std::pair<Token, std::optional<Token>>> params;
    const auto& class_params_opt = child(node, 2);
    if (!class_params_opt.children.empty() && !is_epsilon_node(child(class_params_opt, 0))) {
        params = build_arg_id_list_opt(child(class_params_opt, 1));
    }

    // ClassInheritanceOpt -> INHERITS IDENTIFIER ClassBaseArgsOpt | ε
    std::optional<Token> parent_name = std::nullopt;
    std::vector<ExprPtr> parent_args;
    const auto& inheritance_opt = child(node, 3);
    if (!inheritance_opt.children.empty() && !is_epsilon_node(child(inheritance_opt, 0))) {
        parent_name = child(inheritance_opt, 1).token;
        const auto& base_args_opt = child(inheritance_opt, 2);
        if (!base_args_opt.children.empty() && !is_epsilon_node(child(base_args_opt, 0))) {
            // ClassBaseArgsOpt -> LPAREN ArgListOpt RPAREN; ArgListOpt son expresiones (8b).
            parent_args = build_arg_list_opt(child(base_args_opt, 1));
        }
    }

    std::vector<AttributeDef> attributes;
    std::vector<MethodDef> methods;
    extract_class_body(child(node, 5), attributes, methods);

    return std::make_unique<ClassDecl>(
        std::move(name), std::move(params),
        std::move(parent_name), std::move(parent_args),
        std::move(attributes), std::move(methods));
}

Token build_type_annotation(const CstNode& node) {
    expect_symbol(node, "TypeAnnotation");
    return child(node, 1).token;
}

AttributeDef build_class_attr_from_head(const CstNode& head_node, Token name) {
    expect_symbol(head_node, "ClassAttrListHead");
    const auto& first_child = child(head_node, 0);
    if (first_child.symbol == "TypeAnnotation") {
        auto declared_type = build_type_annotation(first_child);
        ExprPtr value = build_expr(child(head_node, 2));
        return {std::move(name), std::move(declared_type), std::move(value)};
    }
    if (first_child.token.type == TokenType::EQUAL) {
        ExprPtr value = build_expr(child(head_node, 1));
        return {std::move(name), std::nullopt, std::move(value)};
    }
    throw std::runtime_error("Se esperaba atributo con '=' en ClassAttrListHead");
}

MethodDef build_class_method_from_head(const CstNode& head_node, Token name) {
    expect_symbol(head_node, "ClassAttrListHead");
    const auto& first_child = child(head_node, 0);
    if (first_child.symbol != "LPAREN") {
        throw std::runtime_error("Se esperaba metodo en ClassAttrListHead");
    }
    auto params = build_arg_id_list_opt(child(head_node, 1));
    auto return_type = build_method_return_type_opt(child(head_node, 3));
    ExprPtr body = build_method_body(child(head_node, 4));
    return {std::move(name), std::move(params), std::move(return_type), std::move(body)};
}

void extract_class_attr_list_head(
    const CstNode& head_node,
    Token name,
    std::vector<AttributeDef>& attributes,
    std::vector<MethodDef>& methods) {
    if (child(head_node, 0).symbol == "TypeAnnotation" ||
        child(head_node, 0).token.type == TokenType::EQUAL) {
        attributes.push_back(build_class_attr_from_head(head_node, std::move(name)));
        return;
    }
    methods.push_back(build_class_method_from_head(head_node, std::move(name)));
    extract_class_method_list(child(head_node, 5), methods);
}

void extract_class_attr_list(const CstNode& node, std::vector<AttributeDef>& attributes, std::vector<MethodDef>& methods) {
    expect_symbol(node, "ClassAttrList");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }

    Token member_name = child(node, 0).token;
    extract_class_attr_list_head(child(node, 1), std::move(member_name), attributes, methods);
    extract_class_attr_list(child(node, 2), attributes, methods);
}

MethodDef build_class_method(const CstNode& node) {
    expect_symbol(node, "ClassMethod");
    // IDENTIFIER LPAREN ArgIdListOpt RPAREN MethodReturnTypeOpt MethodBody
    Token name = child(node, 0).token;
    auto params = build_arg_id_list_opt(child(node, 2));
    auto return_type = build_method_return_type_opt(child(node, 4));
    ExprPtr body = build_method_body(child(node, 5));
    return {std::move(name), std::move(params), std::move(return_type), std::move(body)};
}

void extract_class_method_list(const CstNode& node, std::vector<MethodDef>& methods) {
    expect_symbol(node, "ClassMethodList");
    if (node.children.empty() || is_epsilon_node(child(node, 0))) {
        return;
    }

    methods.push_back(build_class_method(child(node, 0)));
    extract_class_method_list(child(node, 1), methods);
}

void extract_class_body(const CstNode& node, std::vector<AttributeDef>& attributes, std::vector<MethodDef>& methods) {
    expect_symbol(node, "ClassBody");
    extract_class_attr_list(child(node, 0), attributes, methods);
    extract_class_method_list(child(node, 1), methods);
}

}  // namespace

ProgramPtr cst_to_ast(const CstNode& root) {
    expect_symbol(root, "Program");
    std::vector<StmtPtr> stmts;
    extract_stmt_list(child(root, 0), stmts);
    return std::make_unique<Program>(std::move(stmts));
}

}  // namespace parser
