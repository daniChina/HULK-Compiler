#include <exception>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "../../Parser/ast/cst_to_ast.hpp"
#include "../../Parser/core/token_adapter.hpp"
#include "../../Parser/generator/first_follow.hpp"
#include "../../Parser/generator/grammar_reader.hpp"
#include "../../Parser/generator/ll1_table.hpp"
#include "../../Parser/syntax/ll1_parser.hpp"
#include "../phase2_checker.hpp"

namespace {

parser::ProgramPtr parse_program(const std::string& source) {
    const auto grammar = parser::generator::read_grammar_file("Parser/grammar/grammar.ll1");
    const auto first_follow = parser::generator::compute_first_follow(grammar);
    const auto ll1_table = parser::generator::build_ll1_table(grammar, first_follow);

    std::istringstream input(source);
    auto tokens = parser::tokenize_stream(input);
    parser::Ll1Parser parser_engine(std::move(tokens), grammar, ll1_table.table);
    const auto parse_result = parser_engine.parse();
    return parser::cst_to_ast(*parse_result.cst_root);
}

void collect_exprs(parser::Expr* expr, std::vector<parser::Expr*>& out) {
    if (!expr) {
        return;
    }
    out.push_back(expr);

    if (auto* grouped = dynamic_cast<parser::GroupedExpr*>(expr)) {
        collect_exprs(grouped->expression.get(), out);
    } else if (auto* unary = dynamic_cast<parser::UnaryExpr*>(expr)) {
        collect_exprs(unary->right.get(), out);
    } else if (auto* binary = dynamic_cast<parser::BinaryExpr*>(expr)) {
        collect_exprs(binary->left.get(), out);
        collect_exprs(binary->right.get(), out);
    } else if (auto* call = dynamic_cast<parser::CallExpr*>(expr)) {
        collect_exprs(call->callee.get(), out);
        for (const auto& arg : call->args) {
            collect_exprs(arg.get(), out);
        }
    } else if (auto* let = dynamic_cast<parser::LetExpr*>(expr)) {
        collect_exprs(let->initializer.get(), out);
        collect_exprs(let->body.get(), out);
    } else if (auto* get_attr = dynamic_cast<parser::GetAttrExpr*>(expr)) {
        collect_exprs(get_attr->object.get(), out);
    } else if (auto* method_call = dynamic_cast<parser::MethodCallExpr*>(expr)) {
        collect_exprs(method_call->object.get(), out);
        for (const auto& arg : method_call->args) {
            collect_exprs(arg.get(), out);
        }
    }
}

bool expect_type(const char* name, const TypeInfo& actual, TypeInfo::Kind expected) {
    if (actual.getKind() != expected) {
        std::cerr << "[FAIL] " << name << " — tipo " << actual.toString() << " vs esperado "
                  << TypeInfo(expected).toString() << "\n";
        return false;
    }
    std::cout << "[OK] " << name << " -> " << actual.toString() << "\n";
    return true;
}

bool run_smoke() {
    try {
        const auto program = parse_program("print(1 + 2);\n");
        semantic::Phase2Analyzer analyzer;
        analyzer.analyze(program.get());
        if (analyzer.hasErrors()) {
            analyzer.printErrors();
            std::cerr << "[FAIL] programa valido no debe tener errores semanticos\n";
            return false;
        }

        const auto& type_map = analyzer.getTypeMap();
        if (type_map.size() == 0) {
            std::cerr << "[FAIL] TypeMap vacio tras analyze()\n";
            return false;
        }

        parser::Expr* root = nullptr;
        for (const auto& stmt : program->stmts) {
            if (auto* expr_stmt = dynamic_cast<parser::ExprStmt*>(stmt.get())) {
                root = expr_stmt->expr.get();
                break;
            }
        }
        if (!root) {
            std::cerr << "[FAIL] no se encontro ExprStmt\n";
            return false;
        }

        std::vector<parser::Expr*> exprs;
        collect_exprs(root, exprs);

        bool ok = true;
        for (parser::Expr* expr : exprs) {
            if (!type_map.has(expr)) {
                std::cerr << "[FAIL] expr sin entrada en TypeMap\n";
                ok = false;
            }
        }

        auto* call = dynamic_cast<parser::CallExpr*>(root);
        if (!call) {
            std::cerr << "[FAIL] se esperaba CallExpr en raiz\n";
            return false;
        }

        ok &= expect_type("print call", type_map.get(call), TypeInfo::Kind::Void);

        auto* add = dynamic_cast<parser::BinaryExpr*>(call->args[0].get());
        if (!add) {
            std::cerr << "[FAIL] se esperaba BinaryExpr en arg de print\n";
            return false;
        }
        ok &= expect_type("1 + 2", type_map.get(add), TypeInfo::Kind::Number);
        ok &= expect_type("literal 1", type_map.get(add->left.get()), TypeInfo::Kind::Number);
        ok &= expect_type("literal 2", type_map.get(add->right.get()), TypeInfo::Kind::Number);

        std::cout << "[OK] TypeMap size=" << type_map.size() << " exprs visitados=" << exprs.size()
                  << "\n";
        return ok;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] excepcion: " << error.what() << "\n";
        return false;
    }
}

}  // namespace

int main() {
    return run_smoke() ? 0 : 1;
}
