#pragma once
#include "../Parser/ast/expr.hpp"
#include "../Parser/ast/visitor.hpp"

using BooleanExpr = parser::BoolExpr;
using VariableExpr = parser::IdentifierExpr;
using ExprBlock = parser::BlockExpr;

// Expose parser namespace classes to global namespace
using parser::Program;
using parser::Stmt;
using parser::Expr;
using parser::NumberExpr;
using parser::StringExpr;
using parser::NullExpr;
using parser::SelfExpr;
using parser::UnaryExpr;
using parser::BinaryExpr;
using parser::CallExpr;
using parser::LetExpr;
using parser::AssignExpr;
using parser::FunctionDecl;
using parser::IfExpr;
using parser::WhileExpr;
using parser::ForExpr;
using parser::TypeDecl;
using parser::AttributeDef;
using parser::MethodDef;
using parser::NewExpr;
using parser::GetAttrExpr;
using parser::SetAttrExpr;
using parser::MethodCallExpr;
using parser::BaseCallExpr;
using parser::IsExpr;
using parser::AsExpr;
using parser::UnlessExpr;
using parser::RepeatExpr;
using parser::LoopWhileExpr;
using parser::ExprVisitor;
using parser::StmtVisitor;
using parser::ExprStmt;
using parser::AttributeDecl;
using parser::MethodDecl;
