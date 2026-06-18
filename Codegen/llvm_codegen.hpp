#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>

#include "../Parser/ast/expr.hpp"
#include "../Parser/ast/visitor.hpp"

namespace llvm {
class AllocaInst;
class Function;
class GlobalVariable;
class StructType;
class Type;
class Value;
}  // namespace llvm

namespace hulk::codegen {

struct LLVMScope {
    std::unordered_map<std::string, llvm::AllocaInst*> variables;
    std::unordered_map<std::string, llvm::Type*> variable_types;
    LLVMScope* parent = nullptr;

    explicit LLVMScope(LLVMScope* parent_scope = nullptr) : parent(parent_scope) {}
};

class LLVMCodeGenerator final : public parser::ExprVisitor, public parser::StmtVisitor {
public:
    LLVMCodeGenerator();
    ~LLVMCodeGenerator() override;

    void initialize(const std::string& module_name);
    void generate(parser::Program* program);
    std::string getIR() const;

    bool hadError() const { return had_error_; }
    const std::string& lastError() const { return last_error_; }

    void visit(parser::Program* stmt) override;
    void visit(parser::ExprStmt* stmt) override;
    void visit(parser::FunctionDecl* stmt) override;
    void visit(parser::ClassDecl* stmt) override;
    void visit(parser::MethodDecl* stmt) override;
    void visit(parser::AttributeDecl* stmt) override;

    void visit(parser::NumberExpr* expr) override;
    void visit(parser::StringExpr* expr) override;
    void visit(parser::NullExpr* expr) override;
    void visit(parser::BoolExpr* expr) override;
    void visit(parser::IdentifierExpr* expr) override;
    void visit(parser::SelfExpr* expr) override;
    void visit(parser::GroupedExpr* expr) override;
    void visit(parser::UnaryExpr* expr) override;
    void visit(parser::BinaryExpr* expr) override;
    void visit(parser::CallExpr* expr) override;
    void visit(parser::GetAttrExpr* expr) override;
    void visit(parser::LetExpr* expr) override;
    void visit(parser::BlockExpr* expr) override;
    void visit(parser::IfExpr* expr) override;
    void visit(parser::WhileExpr* expr) override;
    void visit(parser::ForExpr* expr) override;
    void visit(parser::WithExpr* expr) override;
    void visit(parser::CaseExpr* expr) override;
    void visit(parser::AsExpr* expr) override;
    void visit(parser::AssignExpr* expr) override;
    void visit(parser::NewExpr* expr) override;
    void visit(parser::BaseCallExpr* expr) override;
    void visit(parser::SetAttrExpr* expr) override;
    void visit(parser::MethodCallExpr* expr) override;
    void visit(parser::UnlessExpr* expr) override;
    void visit(parser::RepeatExpr* expr) override;
    void visit(parser::LoopWhileExpr* expr) override;

private:
    void fail(const std::string& message);
    void createEmptyMain();
    void registerMathematicalConstants();
    void registerPrintDeclarations();
    void registerRangeDeclarations();
    void materializeExprResult();
    void enterScope();
    void exitScope();
    bool lookupLocalVariable(const std::string& name, llvm::AllocaInst** out_alloca, llvm::Type** out_type);
    llvm::StructType* getBoxedValueType();
    llvm::StructType* getRangeType();
    llvm::GlobalVariable* registerStringConstant(const std::string& value);
    llvm::Value* createBoxedFromDouble(llvm::Value* double_val);
    llvm::Value* createBoxedFromBool(llvm::Value* bool_val);
    llvm::Value* createBoxedFromString(llvm::Value* str_val);
    void emitLogicalAndShortCircuit(parser::Expr* right_expr);
    void emitLogicalOrShortCircuit(parser::Expr* right_expr);
    llvm::Function* getLibmFunction(const char* name, llvm::FunctionType* fn_type);
    llvm::Value* expectBoolValue(llvm::Value* value, const std::string& op);
    llvm::Value* expectDoubleValue(llvm::Value* value, const std::string& op);
    llvm::Value* defaultValueForType(llvm::Type* type);
    void emitPrintValue(llvm::Value* value);
    void emitPrintNewline();
    bool isBoxedValuePointer(llvm::Type* type);
    bool isRangePointer(llvm::Type* type);
    bool emitBuiltinCall(const std::string& name, const std::vector<parser::ExprPtr>& args);
    static bool isBuiltinFunctionName(const std::string& name);
    static std::string mangleUserFunctionName(const std::string& name, std::size_t arity);
    void declareUserFunction(parser::FunctionDecl* decl);
    void defineUserFunction(parser::FunctionDecl* decl);
    llvm::Function* lookupUserFunction(const std::string& name, std::size_t arity);
    llvm::Value* invokeUserFunction(llvm::Function* fn, const std::vector<llvm::Value*>& args);

    std::unique_ptr<llvm::LLVMContext> context_;
    std::unique_ptr<llvm::Module> module_;
    std::unique_ptr<llvm::IRBuilder<>> builder_;

    llvm::Value* current_value_ = nullptr;
    int var_counter_ = 0;
    llvm::StructType* boxed_value_type_ = nullptr;
    llvm::StructType* range_type_ = nullptr;

    std::unordered_map<std::string, llvm::GlobalVariable*> string_constants_;
    std::unordered_map<std::string, llvm::GlobalVariable*> global_constants_;
    std::unordered_map<std::string, std::unordered_map<std::size_t, llvm::Function*>> user_functions_;
    std::vector<std::string> user_function_names_;

    std::vector<std::unique_ptr<LLVMScope>> scopes_;
    LLVMScope* current_scope_ = nullptr;

    bool had_error_ = false;
    std::string last_error_;
};

}  // namespace hulk::codegen
