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
#include "../SymbolTable/symbol_table.hpp"
#include "../Types/type_info.hpp"

namespace llvm {
class AllocaInst;
class Function;
class GlobalVariable;
class PointerType;
class StructType;
class Type;
class Value;
}  // namespace llvm

namespace hulk::codegen {

struct ClassInfo {
    parser::ClassDecl* decl = nullptr;
    llvm::StructType* struct_ty = nullptr;
    std::vector<std::string> field_names;
    std::vector<llvm::Type*> field_llvm_types;
    std::unordered_map<std::string, unsigned> field_index;
};

struct MethodResolution {
    const parser::MethodDef* method = nullptr;
    parser::ClassDecl* declaring_class = nullptr;
};

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
    void setSymbolTable(const SymbolTable* symbol_table);
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
    void visit(parser::IsExpr* expr) override;
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
    void registerValueHelperDeclarations();
    void registerRangeDeclarations();
    void materializeExprResult();
    void enterScope();
    void exitScope();
    bool lookupLocalVariable(const std::string& name, llvm::AllocaInst** out_alloca, llvm::Type** out_type);
    llvm::StructType* getBoxedValueType();
    llvm::StructType* getRangeType();
    llvm::PointerType* opaquePtr();
    llvm::Type* classifySemanticType(llvm::Value* value);
    bool lookupSemanticTypeForAlloca(llvm::AllocaInst* alloca, llvm::Type** out_type);
    bool isBoxedSemanticType(llvm::Type* type);
    bool isRangeSemanticType(llvm::Type* type);
    bool isBoxedValue(llvm::Value* value);
    bool isRangeValue(llvm::Value* value);
    llvm::GlobalVariable* registerStringConstant(const std::string& value);
    llvm::Value* createBoxedFromDouble(llvm::Value* double_val);
    llvm::Value* createBoxedFromBool(llvm::Value* bool_val);
    llvm::Value* createBoxedFromString(llvm::Value* str_val);
    void emitLogicalAndShortCircuit(parser::Expr* right_expr);
    void emitLogicalOrShortCircuit(parser::Expr* right_expr);
    llvm::Function* getLibmFunction(const char* name, llvm::FunctionType* fn_type);
    llvm::Value* expectBoolValue(llvm::Value* value, const std::string& op);
    llvm::Value* expectDoubleValue(llvm::Value* value, const std::string& op);
    llvm::Value* unboxDoubleValue(llvm::Value* value, const std::string& op);
    llvm::Value* unboxBoolValue(llvm::Value* value, const std::string& op);
    llvm::Value* emitBoxedEquality(llvm::Value* left, llvm::Value* right, bool equal);
    llvm::Value* coerceStringBoxed(llvm::Value* value, const std::string& op);
    llvm::Type* semanticTypeForHulkType(const std::optional<parser::Token>& declared_type);
    TypeInfo typeInfoFromHulkToken(const std::optional<parser::Token>& declared_type) const;
    llvm::Type* llvmTypeForTypeInfo(const TypeInfo& type);
    llvm::Type* semanticTypeForTypeInfo(const TypeInfo& type);
    llvm::Value* coerceToLlvmType(llvm::Value* value, llvm::Type* target);
    llvm::Value* materializeToOpaquePtr(llvm::Value* value, const TypeInfo& ret_type,
                                        const std::string& context = "");
    llvm::Value* demoteFromCallResult(llvm::Value* value, const TypeInfo& expected);
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
    std::shared_ptr<FunctionSymbol> lookupUserFunctionSymbol(const std::string& name,
                                                             std::size_t arity) const;
    llvm::Value* invokeUserFunction(llvm::Function* fn, const std::vector<llvm::Value*>& args);
    void registerClassDecl(parser::ClassDecl* decl);
    void buildAllClassStructTypes();
    parser::ClassDecl* getParentClass(parser::ClassDecl* decl);
    llvm::Type* llvmTypeForHulkType(const std::optional<parser::Token>& declared_type);
    bool isStringHulkType(const std::optional<parser::Token>& declared_type);
    ClassInfo* lookupClassInfoByStruct(llvm::StructType* struct_ty);
    MethodResolution resolveMethod(parser::ClassDecl* type_def, const std::string& method_name) const;
    bool instanceConformsTo(parser::ClassDecl* dynamic_type, const std::string& static_type_name) const;
    llvm::GlobalVariable* getClassNameGlobal(const std::string& class_name);
    llvm::Value* loadInstanceRuntimeType(llvm::Value* instance_ptr, llvm::StructType* struct_ty);
    void storeInstanceRuntimeType(llvm::Value* instance_ptr, llvm::StructType* struct_ty,
                                  const std::string& class_name);
    llvm::Value* emitRuntimeTypeConforms(llvm::Value* runtime_type_ptr, const std::string& target_type);
    llvm::Value* emitScalarIsCheck(llvm::Value* value, const std::string& target_type);
    bool isInstanceValue(llvm::Value* value);
    llvm::StructType* resolveInstanceStructType(llvm::Value* value);
    void emitRuntimeErrorAt(int line, int col, const char* message);
    void emitCastFailure(int line = 0, int col = 0);
    void emitCaseFailure(int line = 0, int col = 0);
    void trackInstanceType(llvm::Value* value, llvm::StructType* struct_ty);
    void initializeParentAttributes(llvm::Value* instance_ptr, llvm::StructType* struct_ty,
                                    parser::ClassDecl* type_def);
    llvm::Value* materializeMethodResult(llvm::Value* result, const TypeInfo& ret_type,
                                         const std::string& label);
    llvm::Value* expectFieldValue(llvm::Value* value, llvm::Type* field_ty, const std::string& op);
    ClassInfo* lookupClassInfo(const std::string& name);
    llvm::StructType* getInstanceStructType(const std::string& class_name);
    bool isInstanceSemanticType(llvm::Type* type);
    llvm::Value* resolveInstancePtr(parser::Expr* object_expr, llvm::StructType** out_struct);
    llvm::Value* loadInstanceField(llvm::Value* instance_ptr, llvm::StructType* struct_ty,
                                   parser::ClassDecl* class_decl, const std::string& field);
    bool lookupFieldIndex(ClassInfo* info, const std::string& field, unsigned* out_idx);
    void storeInstanceField(llvm::Value* instance_ptr, llvm::StructType* struct_ty,
                            parser::ClassDecl* class_decl, const std::string& field, llvm::Value* value);
    void declareClassMethods(parser::ClassDecl* decl);
    void defineClassMethods(parser::ClassDecl* decl);
    void defineMethod(parser::ClassDecl* class_decl, const parser::MethodDef& method);
    const parser::MethodDef* findMethod(parser::ClassDecl* type_def, const std::string& method_name) const;
    void buildClassStructType(parser::ClassDecl* decl);
    llvm::Function* lookupMethod(const std::string& class_name, const std::string& method_name,
                                 std::size_t arity);
    std::string mangleMethodName(const std::string& class_name, const std::string& method_name,
                                 std::size_t arity);
    void emitMethodCall(parser::Expr* object_expr, const std::string& method_name,
                        const std::vector<parser::ExprPtr>& args);
    void emitMethodCallOnInstance(llvm::Value* instance_ptr, llvm::StructType* struct_ty,
                                  const std::string& method_name,
                                  const std::vector<parser::ExprPtr>& args);
    llvm::Value* boxValueForStorage(llvm::Value* value);

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
    std::unordered_map<std::string, parser::ClassDecl*> class_decls_;
    std::unordered_map<std::string, ClassInfo> class_info_;
    std::unordered_map<std::string, std::unordered_map<std::string, llvm::Function*>> class_methods_;
    std::vector<std::string> method_mangled_names_;
    std::unordered_map<std::string, llvm::GlobalVariable*> class_name_globals_;
    std::unordered_map<llvm::Value*, llvm::StructType*> value_instance_types_;
    const SymbolTable* symbol_table_ = nullptr;
    llvm::AllocaInst* current_self_alloca_ = nullptr;
    parser::ClassDecl* current_self_class_ = nullptr;
    std::string current_method_name_;

    std::vector<std::unique_ptr<LLVMScope>> scopes_;
    LLVMScope* current_scope_ = nullptr;

    bool had_error_ = false;
    std::string last_error_;
};

}  // namespace hulk::codegen
