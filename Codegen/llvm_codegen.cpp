#include "llvm_codegen.hpp"

#include <numeric>
#include <unordered_set>

#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ADT/SmallString.h>
#if __has_include(<llvm/TargetParser/Triple.h>)
#include <llvm/TargetParser/Triple.h>
#else
#include <llvm/ADT/Triple.h>
#endif
#if __has_include(<llvm/TargetParser/Host.h>)
#include <llvm/TargetParser/Host.h>
#else
#include <llvm/Support/Host.h>
#endif
#include <llvm/Support/raw_ostream.h>
#include <llvm/Config/llvm-config.h>

#include "llvm_aux.hpp"

namespace hulk::codegen {
namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kE = 2.71828182845904523536;

bool isLogicalAndOp(const std::string& op) {
    return op == "&&" || op == "and" || op == "&";
}

bool isLogicalOrOp(const std::string& op) {
    return op == "||" || op == "or" || op == "|";
}

#define HULK_VISIT_STUB(Node) \
    void LLVMCodeGenerator::visit(parser::Node* /*node*/) { \
        fail("codegen no implementado para " #Node " (iteracion posterior a I4)"); \
    }

llvm::Function* getMallocFunction(llvm::Module& module, llvm::LLVMContext& context) {
    llvm::Function* malloc_fn = module.getFunction("malloc");
    if (malloc_fn != nullptr) {
        return malloc_fn;
    }
    llvm::FunctionType* malloc_ty = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(context),
        {llvm::Type::getInt64Ty(context)},
        false);
    return llvm::Function::Create(malloc_ty, llvm::Function::ExternalLinkage, "malloc", module);
}

}  // namespace

LLVMCodeGenerator::LLVMCodeGenerator() = default;
LLVMCodeGenerator::~LLVMCodeGenerator() = default;

void LLVMCodeGenerator::fail(const std::string& message) {
    had_error_ = true;
    last_error_ = message;
}

void LLVMCodeGenerator::initialize(const std::string& module_name) {
    context_ = std::make_unique<llvm::LLVMContext>();
    module_ = std::make_unique<llvm::Module>(module_name, *context_);
    module_->setTargetTriple(
#if LLVM_VERSION_MAJOR >= 19
        llvm::Triple(llvm::sys::getDefaultTargetTriple())
#else
        llvm::sys::getDefaultTargetTriple()
#endif
    );
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
    scopes_.clear();
    scopes_.push_back(std::make_unique<LLVMScope>());
    current_scope_ = scopes_.back().get();
    user_functions_.clear();
    user_function_names_.clear();
    class_decls_.clear();
    class_info_.clear();
    class_methods_.clear();
    method_mangled_names_.clear();
    class_name_globals_.clear();
    value_instance_types_.clear();
    symbol_table_ = nullptr;
    current_self_alloca_ = nullptr;
    current_self_class_ = nullptr;
    current_method_name_.clear();
    registerRuntimeDeclarations(*module_, *context_);
    registerMathematicalConstants();
    registerPrintDeclarations();
    registerValueHelperDeclarations();
    registerBuiltinDeclarations(*module_, *context_);
    registerRangeDeclarations();
}

void LLVMCodeGenerator::setSymbolTable(const SymbolTable* symbol_table) {
    symbol_table_ = symbol_table;
    if (symbol_table_ != nullptr) {
        TypeInfo::setSymbolTable(const_cast<SymbolTable*>(symbol_table_));
    }
}

void LLVMCodeGenerator::registerRangeDeclarations() {
    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    llvm::FunctionType* range_create_ty =
        llvm::FunctionType::get(opaquePtr(), {double_ty, double_ty}, false);
    module_->getOrInsertFunction("hulk_range_create", range_create_ty);
}

bool LLVMCodeGenerator::isBuiltinFunctionName(const std::string& name) {
    return name == "print" || name == "sin" || name == "cos" || name == "sqrt" || name == "log" ||
           name == "exp" || name == "rand" || name == "range";
}

std::string LLVMCodeGenerator::mangleUserFunctionName(const std::string& name, std::size_t arity) {
    return "hulk_fn_" + name + "_" + std::to_string(arity);
}

void LLVMCodeGenerator::declareUserFunction(parser::FunctionDecl* decl) {
    const std::string& name = decl->name.lexeme;
    const std::size_t arity = decl->params.size();

    if (isBuiltinFunctionName(name)) {
        fail("No se puede redefinir la funcion builtin '" + name + "'");
        return;
    }

    auto& overloads = user_functions_[name];
    if (overloads.find(arity) != overloads.end()) {
        fail("La funcion '" + name + "' ya esta definida con aridad " + std::to_string(arity));
        return;
    }

    std::vector<llvm::Type*> param_types;
    param_types.reserve(arity);
    if (auto sym = lookupUserFunctionSymbol(name, arity)) {
        for (const TypeInfo& param : sym->parameter_types) {
            param_types.push_back(llvmTypeForTypeInfo(param));
        }
    } else {
        for (const auto& param : decl->params) {
            param_types.push_back(llvmTypeForTypeInfo(typeInfoFromHulkToken(param.second)));
        }
    }

    llvm::FunctionType* fn_type =
        llvm::FunctionType::get(opaquePtr(), param_types, false);
    user_function_names_.push_back(mangleUserFunctionName(name, arity));
    const llvm::StringRef mangled(user_function_names_.back());
    llvm::Function* fn =
        llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, mangled, module_.get());
    overloads[arity] = fn;
}

void LLVMCodeGenerator::defineUserFunction(parser::FunctionDecl* decl) {
    const std::string& name = decl->name.lexeme;
    const std::size_t arity = decl->params.size();
    llvm::Function* fn = lookupUserFunction(name, arity);
    if (fn == nullptr) {
        fail("Funcion interna '" + name + "' no declarada");
        return;
    }
    if (!fn->empty()) {
        return;
    }

    llvm::BasicBlock* saved_insert_block = builder_->GetInsertBlock();
    llvm::Function* saved_insert_fn = saved_insert_block != nullptr ? saved_insert_block->getParent() : nullptr;
    const std::size_t saved_scope_depth = scopes_.size();

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entry);

    scopes_.push_back(std::make_unique<LLVMScope>());
    current_scope_ = scopes_.back().get();

    const std::shared_ptr<FunctionSymbol> sym = lookupUserFunctionSymbol(name, arity);
    const TypeInfo return_type =
        sym ? sym->return_type : typeInfoFromHulkToken(decl->return_type);

    llvm::Function::arg_iterator arg_it = fn->arg_begin();
    for (std::size_t i = 0; i < decl->params.size(); ++i) {
        const auto& param = decl->params[i];
        const std::string& param_name = param.first.lexeme;
        const TypeInfo param_info =
            sym && i < sym->parameter_types.size() ? sym->parameter_types[i]
                                                 : typeInfoFromHulkToken(param.second);
        llvm::Type* param_ty = llvmTypeForTypeInfo(param_info);
        llvm::AllocaInst* alloca = builder_->CreateAlloca(param_ty, nullptr, "");
        builder_->CreateStore(&*arg_it, alloca);
        current_scope_->variables[param_name] = alloca;
        current_scope_->variable_types[param_name] = semanticTypeForTypeInfo(param_info);
        ++arg_it;
    }

    decl->body->accept(this);
    if (had_error_) {
        scopes_.resize(saved_scope_depth);
        current_scope_ = scopes_.back().get();
        if (saved_insert_block != nullptr) {
            builder_->SetInsertPoint(saved_insert_block);
        }
        return;
    }

    llvm::Value* result =
        materializeToOpaquePtr(current_value_, return_type, "funcion '" + name + "'");
    if (result == nullptr) {
        scopes_.resize(saved_scope_depth);
        current_scope_ = scopes_.back().get();
        if (saved_insert_block != nullptr) {
            builder_->SetInsertPoint(saved_insert_block);
        }
        return;
    }

    builder_->CreateRet(result);

    scopes_.resize(saved_scope_depth);
    current_scope_ = scopes_.back().get();
    if (saved_insert_block != nullptr) {
        builder_->SetInsertPoint(saved_insert_block);
    } else if (saved_insert_fn != nullptr) {
        builder_->SetInsertPoint(&saved_insert_fn->getEntryBlock());
    }
}

llvm::Function* LLVMCodeGenerator::lookupUserFunction(const std::string& name, std::size_t arity) {
    const auto fn_it = user_functions_.find(name);
    if (fn_it == user_functions_.end()) {
        return nullptr;
    }
    const auto overload_it = fn_it->second.find(arity);
    if (overload_it == fn_it->second.end()) {
        return nullptr;
    }
    return overload_it->second;
}

std::shared_ptr<FunctionSymbol> LLVMCodeGenerator::lookupUserFunctionSymbol(const std::string& name,
                                                                          std::size_t arity) const {
    if (symbol_table_ == nullptr) {
        return nullptr;
    }
    return symbol_table_->lookupFunction(name, arity);
}

llvm::Value* LLVMCodeGenerator::invokeUserFunction(llvm::Function* fn,
                                                   const std::vector<llvm::Value*>& args) {
    return builder_->CreateCall(fn, args);
}

std::string LLVMCodeGenerator::mangleMethodName(const std::string& class_name,
                                                const std::string& method_name,
                                                std::size_t arity) {
    return "hulk_meth_" + class_name + "_" + method_name + "_" + std::to_string(arity);
}

llvm::Type* LLVMCodeGenerator::llvmTypeForHulkType(const std::optional<parser::Token>& declared_type) {
    return llvmTypeForTypeInfo(typeInfoFromHulkToken(declared_type));
}

TypeInfo LLVMCodeGenerator::typeInfoFromHulkToken(const std::optional<parser::Token>& declared_type) const {
    if (!declared_type.has_value()) {
        return TypeInfo(TypeInfo::Kind::Unknown);
    }
    const std::string& name = declared_type->lexeme;
    if (name == "Number") {
        return TypeInfo::Number();
    }
    if (name == "String") {
        return TypeInfo::String();
    }
    if (name == "Boolean") {
        return TypeInfo::Boolean();
    }
    if (name == "Void") {
        return TypeInfo(TypeInfo::Kind::Void);
    }
    if (name == "Object") {
        return TypeInfo::Object();
    }
    return TypeInfo::Object(name);
}

llvm::Type* LLVMCodeGenerator::llvmTypeForTypeInfo(const TypeInfo& type) {
    switch (type.getKind()) {
    case TypeInfo::Kind::Number:
    case TypeInfo::Kind::Unknown:
        return llvm::Type::getDoubleTy(*context_);
    case TypeInfo::Kind::Boolean:
        return llvm::Type::getInt1Ty(*context_);
    case TypeInfo::Kind::String:
    case TypeInfo::Kind::Null:
    case TypeInfo::Kind::Object:
    case TypeInfo::Kind::Void:
    case TypeInfo::Kind::Function:
    default:
        return opaquePtr();
    }
}

llvm::Type* LLVMCodeGenerator::semanticTypeForTypeInfo(const TypeInfo& type) {
    switch (type.getKind()) {
    case TypeInfo::Kind::String:
        return getBoxedValueType();
    case TypeInfo::Kind::Object: {
        const std::string& name = type.getTypeName();
        if (!name.empty()) {
            llvm::StructType* inst_ty = getInstanceStructType(name);
            if (inst_ty != nullptr) {
                return inst_ty;
            }
        }
        return opaquePtr();
    }
    case TypeInfo::Kind::Number:
    case TypeInfo::Kind::Unknown:
        return llvm::Type::getDoubleTy(*context_);
    case TypeInfo::Kind::Boolean:
        return llvm::Type::getInt1Ty(*context_);
    default:
        return llvmTypeForTypeInfo(type);
    }
}

llvm::Value* LLVMCodeGenerator::materializeToOpaquePtr(llvm::Value* value, const TypeInfo& ret_type,
                                                       const std::string& context) {
    if (ret_type.getKind() == TypeInfo::Kind::Void) {
        return llvm::ConstantPointerNull::get(opaquePtr());
    }

    if (value == nullptr) {
        return llvm::ConstantPointerNull::get(opaquePtr());
    }

    const llvm::Type* val_ty = value->getType();
    if (val_ty->isPointerTy()) {
        return value;
    }

    const bool infer_number = ret_type.isUnknown() || ret_type.isNumeric();
    const bool infer_bool = ret_type.isUnknown() || ret_type.isBoolean();

    if (val_ty->isDoubleTy() && infer_number) {
        return createBoxedFromDouble(value);
    }
    if (val_ty->isIntegerTy(1) && infer_bool) {
        return createBoxedFromBool(value);
    }

    std::string msg = "codegen: tipo de retorno no soportado";
    if (!context.empty()) {
        msg += " en " + context;
    }
    fail(msg);
    return nullptr;
}

llvm::Value* LLVMCodeGenerator::materializeMethodResult(llvm::Value* result, const TypeInfo& ret_type,
                                                          const std::string& label) {
    return materializeToOpaquePtr(result, ret_type, "metodo '" + label + "'");
}

llvm::Value* LLVMCodeGenerator::coerceToLlvmType(llvm::Value* value, llvm::Type* target) {
    if (target == nullptr) {
        fail("codegen: tipo destino nulo en coerceToLlvmType");
        return nullptr;
    }

    if (value == nullptr) {
        if (target->isPointerTy()) {
            return llvm::ConstantPointerNull::get(opaquePtr());
        }
        fail("codegen: valor nulo en coerceToLlvmType");
        return nullptr;
    }

    llvm::Type* source = value->getType();
    if (source == target) {
        return value;
    }

    if (target == opaquePtr()) {
        return materializeToOpaquePtr(value, TypeInfo(TypeInfo::Kind::Unknown));
    }

    if (target->isDoubleTy()) {
        if (source->isDoubleTy()) {
            return value;
        }
        if (source->isPointerTy()) {
            llvm::Function* fn = module_->getFunction("hulk_unbox_number");
            return builder_->CreateCall(fn, {value}, "unboxed_num");
        }
        fail("codegen: no se puede convertir a Number");
        return nullptr;
    }

    if (target->isIntegerTy(1)) {
        if (source->isIntegerTy(1)) {
            return value;
        }
        if (source->isPointerTy() && isBoxedValue(value)) {
            return unboxBoolValue(value, "coerce");
        }
        fail("codegen: no se puede convertir a Boolean");
        return nullptr;
    }

    if (target->isPointerTy()) {
        return materializeToOpaquePtr(value, TypeInfo(TypeInfo::Kind::Unknown));
    }

    fail("codegen: coerceToLlvmType no soportado");
    return nullptr;
}

llvm::Value* LLVMCodeGenerator::demoteFromCallResult(llvm::Value* value, const TypeInfo& expected) {
    if (value == nullptr) {
        fail("codegen: valor nulo en demoteFromCallResult");
        return nullptr;
    }

    if (expected.isNumeric()) {
        return coerceToLlvmType(value, llvm::Type::getDoubleTy(*context_));
    }
    if (expected.isBoolean()) {
        return coerceToLlvmType(value, llvm::Type::getInt1Ty(*context_));
    }
    if (expected.getKind() == TypeInfo::Kind::Void) {
        return llvm::ConstantPointerNull::get(opaquePtr());
    }

    if (expected.isString() || expected.isObject() || expected.isNull() || expected.isUnknown()) {
        if (value->getType()->isPointerTy()) {
            if (expected.isObject()) {
                const std::string& type_name = expected.getTypeName();
                if (!type_name.empty()) {
                    if (llvm::StructType* struct_ty = getInstanceStructType(type_name)) {
                        trackInstanceType(value, struct_ty);
                    }
                }
            }
            return value;
        }
    }

    fail("codegen: demoteFromCallResult no soportado");
    return nullptr;
}

llvm::Type* LLVMCodeGenerator::semanticTypeForHulkType(const std::optional<parser::Token>& declared_type) {
    return semanticTypeForTypeInfo(typeInfoFromHulkToken(declared_type));
}

bool LLVMCodeGenerator::isStringHulkType(const std::optional<parser::Token>& declared_type) {
    return declared_type.has_value() && declared_type->lexeme == "String";
}

parser::ClassDecl* LLVMCodeGenerator::getParentClass(parser::ClassDecl* decl) {
    if (decl == nullptr || !decl->parent_name.has_value()) {
        return nullptr;
    }
    const auto it = class_decls_.find(decl->parent_name->lexeme);
    if (it == class_decls_.end()) {
        return nullptr;
    }
    return it->second;
}

void LLVMCodeGenerator::registerClassDecl(parser::ClassDecl* decl) {
    const std::string& name = decl->name.lexeme;
    if (class_decls_.find(name) != class_decls_.end()) {
        fail("Tipo redefinido: " + name);
        return;
    }
    class_decls_[name] = decl;
}

void LLVMCodeGenerator::buildClassStructType(parser::ClassDecl* decl) {
    const std::string& name = decl->name.lexeme;
    if (class_info_.find(name) != class_info_.end()) {
        return;
    }

    parser::ClassDecl* parent_decl = getParentClass(decl);
    if (parent_decl != nullptr) {
        buildClassStructType(parent_decl);
    }

    ClassInfo info;
    info.decl = decl;
    std::unordered_set<std::string> seen;

    if (parent_decl != nullptr) {
        ClassInfo* parent_info = lookupClassInfo(parent_decl->name.lexeme);
        if (parent_info != nullptr) {
            info.field_names = parent_info->field_names;
            info.field_llvm_types = parent_info->field_llvm_types;
            for (const auto& field_name : info.field_names) {
                seen.insert(field_name);
            }
        }
    } else {
        info.field_names.push_back("__hulk_rt_type__");
        info.field_llvm_types.push_back(opaquePtr());
    }

    for (const auto& param : decl->params) {
        const std::string& field_name = param.first.lexeme;
        if (seen.insert(field_name).second) {
            info.field_names.push_back(field_name);
            info.field_llvm_types.push_back(llvmTypeForHulkType(param.second));
        }
    }
    for (const auto& attr : decl->attributes) {
        const std::string& field_name = attr.name.lexeme;
        if (seen.insert(field_name).second) {
            info.field_names.push_back(field_name);
            info.field_llvm_types.push_back(llvmTypeForHulkType(attr.declared_type));
        }
    }

    info.struct_ty = llvm::StructType::create(*context_, info.field_llvm_types, "HulkInstance_" + name);
    for (std::size_t i = 0; i < info.field_names.size(); ++i) {
        info.field_index[info.field_names[i]] = static_cast<unsigned>(i);
    }
    class_info_[name] = std::move(info);
}

void LLVMCodeGenerator::buildAllClassStructTypes() {
    for (const auto& [name, decl] : class_decls_) {
        (void)name;
        buildClassStructType(decl);
        if (had_error_) {
            return;
        }
    }
}

ClassInfo* LLVMCodeGenerator::lookupClassInfoByStruct(llvm::StructType* struct_ty) {
    if (struct_ty == nullptr) {
        return nullptr;
    }
    for (auto& [class_name, info] : class_info_) {
        (void)class_name;
        if (info.struct_ty == struct_ty) {
            return &info;
        }
    }
    return nullptr;
}

llvm::Value* LLVMCodeGenerator::expectFieldValue(llvm::Value* value, llvm::Type* field_ty,
                                                 const std::string& op) {
    if (value == nullptr) {
        fail("codegen: valor nulo en " + op);
        return nullptr;
    }
    if (field_ty->isDoubleTy()) {
        return expectDoubleValue(value, op);
    }
    if (field_ty->isIntegerTy(1)) {
        return expectBoolValue(value, op);
    }
    if (field_ty == opaquePtr()) {
        if (!value->getType()->isPointerTy()) {
            fail("codegen: se esperaba string en " + op);
            return nullptr;
        }
        return value;
    }
    fail("codegen: tipo de campo no soportado en " + op);
    return nullptr;
}

ClassInfo* LLVMCodeGenerator::lookupClassInfo(const std::string& name) {
    const auto it = class_info_.find(name);
    if (it == class_info_.end()) {
        return nullptr;
    }
    return &it->second;
}

llvm::StructType* LLVMCodeGenerator::getInstanceStructType(const std::string& class_name) {
    ClassInfo* info = lookupClassInfo(class_name);
    if (info == nullptr) {
        return nullptr;
    }
    return info->struct_ty;
}

bool LLVMCodeGenerator::isInstanceSemanticType(llvm::Type* type) {
    if (type == nullptr || !type->isStructTy()) {
        return false;
    }
    const llvm::StructType* struct_ty = llvm::cast<llvm::StructType>(type);
    return struct_ty->hasName() && struct_ty->getName().starts_with("HulkInstance_");
}

llvm::Value* LLVMCodeGenerator::loadInstanceField(llvm::Value* instance_ptr,
                                                  llvm::StructType* struct_ty,
                                                  parser::ClassDecl* class_decl,
                                                  const std::string& field) {
    ClassInfo* info = lookupClassInfo(class_decl->name.lexeme);
    if (info == nullptr) {
        fail("codegen: tipo de instancia no registrado");
        return nullptr;
    }
    const auto field_it = info->field_index.find(field);
    if (field_it == info->field_index.end()) {
        fail("codegen: atributo '" + field + "' no definido en " + class_decl->name.lexeme);
        return nullptr;
    }

    const unsigned idx = field_it->second;
    llvm::Type* field_ty = info->field_llvm_types[idx];
    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_ty, instance_ptr, idx);
    return builder_->CreateLoad(field_ty, field_ptr);
}

bool LLVMCodeGenerator::lookupFieldIndex(ClassInfo* info, const std::string& field,
                                         unsigned* out_idx) {
    if (info == nullptr) {
        fail("codegen: layout de instancia no registrado");
        return false;
    }
    const auto field_it = info->field_index.find(field);
    if (field_it == info->field_index.end()) {
        fail("codegen: atributo '" + field + "' no definido");
        return false;
    }
    *out_idx = field_it->second;
    return true;
}

void LLVMCodeGenerator::storeInstanceField(llvm::Value* instance_ptr, llvm::StructType* struct_ty,
                                           parser::ClassDecl* class_decl, const std::string& field,
                                           llvm::Value* value) {
    ClassInfo* info = lookupClassInfo(class_decl->name.lexeme);
    if (info == nullptr) {
        fail("codegen: tipo de instancia no registrado");
        return;
    }
    const auto field_it = info->field_index.find(field);
    if (field_it == info->field_index.end()) {
        fail("codegen: atributo '" + field + "' no definido en " + class_decl->name.lexeme);
        return;
    }

    const unsigned idx = field_it->second;
    llvm::Type* field_ty = info->field_llvm_types[idx];
    llvm::Value* coerced = expectFieldValue(value, field_ty, field);
    if (coerced == nullptr) {
        return;
    }
    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_ty, instance_ptr, idx);
    builder_->CreateStore(coerced, field_ptr);
}

llvm::Value* LLVMCodeGenerator::resolveInstancePtr(parser::Expr* object_expr,
                                                   llvm::StructType** out_struct) {
    if (object_expr->kind == parser::ExprKind::SELF_REF) {
        if (current_self_alloca_ == nullptr || current_self_class_ == nullptr) {
            fail("codegen: self fuera de metodo");
            return nullptr;
        }
        *out_struct = getInstanceStructType(current_self_class_->name.lexeme);
        return builder_->CreateLoad(opaquePtr(), current_self_alloca_);
    }

    if (object_expr->kind == parser::ExprKind::IDENTIFIER) {
        const auto* id = static_cast<const parser::IdentifierExpr*>(object_expr);
        llvm::AllocaInst* alloca = nullptr;
        llvm::Type* var_type = nullptr;
        if (!lookupLocalVariable(id->token.lexeme, &alloca, &var_type)) {
            fail("codegen: variable '" + id->token.lexeme + "' no definida");
            return nullptr;
        }
        if (!isInstanceSemanticType(var_type)) {
            fail("codegen: no se puede acceder a atributos de un valor no instancia");
            return nullptr;
        }
        *out_struct = llvm::cast<llvm::StructType>(var_type);
        return builder_->CreateLoad(opaquePtr(), alloca);
    }

    object_expr->accept(this);
    if (had_error_) {
        return nullptr;
    }
    llvm::Type* semantic_type = classifySemanticType(current_value_);
    llvm::StructType* tracked_struct = resolveInstanceStructType(current_value_);
    if (tracked_struct != nullptr) {
        *out_struct = tracked_struct;
        return current_value_;
    }
    if (!isInstanceSemanticType(semantic_type)) {
        fail("codegen: no se puede acceder a atributos de un valor no instancia");
        return nullptr;
    }
    *out_struct = llvm::cast<llvm::StructType>(semantic_type);
    return current_value_;
}

const parser::MethodDef* LLVMCodeGenerator::findMethod(parser::ClassDecl* type_def,
                                                       const std::string& method_name) const {
    return resolveMethod(type_def, method_name).method;
}

MethodResolution LLVMCodeGenerator::resolveMethod(parser::ClassDecl* type_def,
                                                  const std::string& method_name) const {
    MethodResolution resolution;
    if (type_def == nullptr) {
        return resolution;
    }
    for (const auto& method : type_def->methods) {
        if (method.name.lexeme == method_name) {
            resolution.method = &method;
            resolution.declaring_class = type_def;
            return resolution;
        }
    }
    parser::ClassDecl* parent = nullptr;
    if (type_def->parent_name.has_value()) {
        const auto parent_it = class_decls_.find(type_def->parent_name->lexeme);
        if (parent_it != class_decls_.end()) {
            parent = parent_it->second;
        }
    }
    if (parent != nullptr) {
        return resolveMethod(parent, method_name);
    }
    return resolution;
}

bool LLVMCodeGenerator::instanceConformsTo(parser::ClassDecl* dynamic_type,
                                           const std::string& static_type_name) const {
    if (dynamic_type == nullptr) {
        return false;
    }
    if (dynamic_type->name.lexeme == static_type_name) {
        return true;
    }
    if (dynamic_type->parent_name.has_value()) {
        const std::string& parent = dynamic_type->parent_name->lexeme;
        if (parent == "Object") {
            return static_type_name == "Object";
        }
        const auto parent_it = class_decls_.find(parent);
        if (parent_it != class_decls_.end()) {
            return instanceConformsTo(parent_it->second, static_type_name);
        }
    }
    return false;
}

llvm::GlobalVariable* LLVMCodeGenerator::getClassNameGlobal(const std::string& class_name) {
    const auto it = class_name_globals_.find(class_name);
    if (it != class_name_globals_.end()) {
        return it->second;
    }
    llvm::GlobalVariable* global = registerStringConstant(class_name);
    class_name_globals_[class_name] = global;
    return global;
}

llvm::Value* LLVMCodeGenerator::loadInstanceRuntimeType(llvm::Value* instance_ptr,
                                                        llvm::StructType* struct_ty) {
    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_ty, instance_ptr, 0);
    return builder_->CreateLoad(opaquePtr(), field_ptr);
}

void LLVMCodeGenerator::storeInstanceRuntimeType(llvm::Value* instance_ptr,
                                                 llvm::StructType* struct_ty,
                                                 const std::string& class_name) {
    llvm::Value* type_name_ptr =
        builder_->CreateBitCast(getClassNameGlobal(class_name), opaquePtr());
    llvm::Value* field_ptr = builder_->CreateStructGEP(struct_ty, instance_ptr, 0);
    builder_->CreateStore(type_name_ptr, field_ptr);
}

llvm::Value* LLVMCodeGenerator::emitRuntimeTypeConforms(llvm::Value* runtime_type_ptr,
                                                         const std::string& target_type) {
    llvm::Value* result = llvm::ConstantInt::getFalse(*context_);
    for (const auto& [class_name, decl] : class_decls_) {
        (void)class_name;
        if (!instanceConformsTo(decl, target_type)) {
            continue;
        }
        llvm::Value* class_name_ptr =
            builder_->CreateBitCast(getClassNameGlobal(decl->name.lexeme), opaquePtr());
        llvm::Value* matches = builder_->CreateICmpEQ(runtime_type_ptr, class_name_ptr);
        result = builder_->CreateOr(result, matches);
    }
    return result;
}

llvm::Value* LLVMCodeGenerator::emitScalarIsCheck(llvm::Value* value, const std::string& target_type) {
    llvm::Type* bool_ty = llvm::Type::getInt1Ty(*context_);
    if (target_type == "Null") {
        return builder_->CreateICmpEQ(value, llvm::ConstantPointerNull::get(opaquePtr()));
    }
    if (target_type == "Number") {
        return llvm::ConstantInt::get(bool_ty, value->getType()->isDoubleTy());
    }
    if (target_type == "Boolean") {
        return llvm::ConstantInt::get(bool_ty, value->getType()->isIntegerTy(1));
    }
    if (target_type == "String") {
        return llvm::ConstantInt::get(bool_ty, isBoxedValue(value));
    }
    return llvm::ConstantInt::getFalse(*context_);
}

bool LLVMCodeGenerator::isInstanceValue(llvm::Value* value) {
    if (value == nullptr || !value->getType()->isPointerTy()) {
        return false;
    }
    if (llvm::isa<llvm::ConstantPointerNull>(value)) {
        return false;
    }
    return isInstanceSemanticType(classifySemanticType(value));
}

llvm::StructType* LLVMCodeGenerator::resolveInstanceStructType(llvm::Value* value) {
    if (auto it = value_instance_types_.find(value); it != value_instance_types_.end()) {
        return it->second;
    }
    llvm::Type* semantic_type = classifySemanticType(value);
    if (!isInstanceSemanticType(semantic_type)) {
        return nullptr;
    }
    return llvm::cast<llvm::StructType>(semantic_type);
}

void LLVMCodeGenerator::trackInstanceType(llvm::Value* value, llvm::StructType* struct_ty) {
    if (value != nullptr && struct_ty != nullptr) {
        value_instance_types_[value] = struct_ty;
    }
}

void LLVMCodeGenerator::emitRuntimeErrorAt(int line, int col, const char* message) {
    llvm::Function* fail_fn = module_->getFunction("hulk_runtime_error_at");
    if (fail_fn == nullptr) {
        llvm::Type* i32_ty = llvm::Type::getInt32Ty(*context_);
        llvm::FunctionType* fail_ty = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*context_), {i32_ty, i32_ty, opaquePtr()}, false);
        fail_fn = llvm::Function::Create(fail_ty, llvm::Function::ExternalLinkage,
                                         "hulk_runtime_error_at", module_.get());
    }
    llvm::Value* line_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), line);
    llvm::Value* col_val = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), col);
    llvm::Value* msg_val = builder_->CreateGlobalStringPtr(message);
    builder_->CreateCall(fail_fn, {line_val, col_val, msg_val});
    builder_->CreateUnreachable();
}

void LLVMCodeGenerator::emitCastFailure(int line, int col) {
    emitRuntimeErrorAt(line, col, "invalid cast");
}

void LLVMCodeGenerator::emitCaseFailure(int line, int col) {
    emitRuntimeErrorAt(line, col, "case sin rama coincidente");
}

llvm::Function* LLVMCodeGenerator::lookupMethod(const std::string& class_name,
                                                const std::string& method_name,
                                                std::size_t arity) {
    const auto class_it = class_methods_.find(class_name);
    if (class_it == class_methods_.end()) {
        return nullptr;
    }
    const std::string key = method_name + "#" + std::to_string(arity);
    const auto method_it = class_it->second.find(key);
    if (method_it == class_it->second.end()) {
        return nullptr;
    }
    return method_it->second;
}

void LLVMCodeGenerator::declareClassMethods(parser::ClassDecl* decl) {
    const std::string& class_name = decl->name.lexeme;
    llvm::Type* self_ty = opaquePtr();

    for (const auto& method : decl->methods) {
        const std::size_t arity = method.params.size();
        std::vector<llvm::Type*> param_types;
        param_types.push_back(self_ty);
        for (const auto& param : method.params) {
            param_types.push_back(llvmTypeForHulkType(param.second));
        }
        llvm::FunctionType* fn_type = llvm::FunctionType::get(opaquePtr(), param_types, false);
        const std::string mangled = mangleMethodName(class_name, method.name.lexeme, arity);
        method_mangled_names_.push_back(mangled);
        llvm::Function* fn = llvm::Function::Create(
            fn_type, llvm::Function::ExternalLinkage, mangled, module_.get());
        class_methods_[class_name][method.name.lexeme + "#" + std::to_string(arity)] = fn;
    }
}

void LLVMCodeGenerator::defineMethod(parser::ClassDecl* class_decl, const parser::MethodDef& method) {
    const std::string& class_name = class_decl->name.lexeme;
    const std::size_t arity = method.params.size();
    llvm::Function* fn = lookupMethod(class_name, method.name.lexeme, arity);
    if (fn == nullptr) {
        fail("Metodo interno '" + method.name.lexeme + "' no declarado");
        return;
    }
    if (!fn->empty()) {
        return;
    }

    llvm::BasicBlock* saved_insert_block = builder_->GetInsertBlock();
    llvm::Function* saved_insert_fn =
        saved_insert_block != nullptr ? saved_insert_block->getParent() : nullptr;
    const std::size_t saved_scope_depth = scopes_.size();
    llvm::AllocaInst* saved_self_alloca = current_self_alloca_;
    parser::ClassDecl* saved_self_class = current_self_class_;
    const std::string saved_method_name = current_method_name_;

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entry);

    scopes_.push_back(std::make_unique<LLVMScope>());
    current_scope_ = scopes_.back().get();

    llvm::Function::arg_iterator arg_it = fn->arg_begin();
    llvm::Value* self_arg = &*arg_it;
    ++arg_it;

    current_self_alloca_ = builder_->CreateAlloca(opaquePtr(), nullptr, "");
    builder_->CreateStore(self_arg, current_self_alloca_);
    current_self_class_ = class_decl;
    current_method_name_ = method.name.lexeme;

    for (const auto& param : method.params) {
        llvm::Type* param_ty = llvmTypeForHulkType(param.second);
        llvm::AllocaInst* alloca = builder_->CreateAlloca(param_ty, nullptr, "");
        builder_->CreateStore(&*arg_it, alloca);
        current_scope_->variables[param.first.lexeme] = alloca;
        current_scope_->variable_types[param.first.lexeme] = semanticTypeForHulkType(param.second);
        ++arg_it;
    }

    method.body->accept(this);
    if (had_error_) {
        scopes_.resize(saved_scope_depth);
        current_scope_ = scopes_.back().get();
        current_self_alloca_ = saved_self_alloca;
        current_self_class_ = saved_self_class;
        current_method_name_ = saved_method_name;
        if (saved_insert_block != nullptr) {
            builder_->SetInsertPoint(saved_insert_block);
        }
        return;
    }

    llvm::Value* result =
        materializeMethodResult(current_value_, typeInfoFromHulkToken(method.return_type), method.name.lexeme);
    if (result == nullptr) {
        scopes_.resize(saved_scope_depth);
        current_scope_ = scopes_.back().get();
        current_self_alloca_ = saved_self_alloca;
        current_self_class_ = saved_self_class;
        current_method_name_ = saved_method_name;
        if (saved_insert_block != nullptr) {
            builder_->SetInsertPoint(saved_insert_block);
        }
        return;
    }

    builder_->CreateRet(result);

    scopes_.resize(saved_scope_depth);
    current_scope_ = scopes_.back().get();
    current_self_alloca_ = saved_self_alloca;
    current_self_class_ = saved_self_class;
    current_method_name_ = saved_method_name;
    if (saved_insert_block != nullptr) {
        builder_->SetInsertPoint(saved_insert_block);
    } else if (saved_insert_fn != nullptr) {
        builder_->SetInsertPoint(&saved_insert_fn->getEntryBlock());
    }
}

void LLVMCodeGenerator::defineClassMethods(parser::ClassDecl* decl) {
    for (const auto& method : decl->methods) {
        defineMethod(decl, method);
        if (had_error_) {
            return;
        }
    }
}

void LLVMCodeGenerator::emitMethodCallOnInstance(llvm::Value* instance_ptr,
                                                 llvm::StructType* struct_ty,
                                                 const std::string& method_name,
                                                 const std::vector<parser::ExprPtr>& args) {
    ClassInfo* info = lookupClassInfoByStruct(struct_ty);
    if (info == nullptr || info->decl == nullptr) {
        fail("codegen: tipo de instancia desconocido para llamada a metodo");
        return;
    }

    const MethodResolution static_resolution = resolveMethod(info->decl, method_name);
    if (static_resolution.method == nullptr) {
        fail("Metodo no definido: " + method_name);
        return;
    }
    if (static_resolution.method->params.size() != args.size()) {
        fail("El metodo '" + method_name + "' espera " +
             std::to_string(static_resolution.method->params.size()) + " argumento(s)");
        return;
    }

    std::vector<parser::ClassDecl*> dispatch_classes;
    dispatch_classes.reserve(class_decls_.size());
    for (const auto& [class_name, decl] : class_decls_) {
        (void)class_name;
        const MethodResolution candidate = resolveMethod(decl, method_name);
        if (candidate.method != nullptr && candidate.method->params.size() == args.size()) {
            dispatch_classes.push_back(decl);
        }
    }
    if (dispatch_classes.empty()) {
        fail("Metodo no definido: " + method_name);
        return;
    }

    std::vector<llvm::Value*> arg_values;
    arg_values.reserve(args.size());
    for (std::size_t i = 0; i < args.size(); ++i) {
        args[i]->accept(this);
        if (had_error_) {
            return;
        }
        llvm::Type* param_ty = llvmTypeForHulkType(static_resolution.method->params[i].second);
        llvm::Value* value = expectFieldValue(current_value_, param_ty, method_name);
        if (value == nullptr) {
            return;
        }
        arg_values.push_back(value);
    }

    llvm::Value* runtime_type = loadInstanceRuntimeType(instance_ptr, struct_ty);

    llvm::Function* parent_fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "meth.merge", parent_fn);
    llvm::BasicBlock* next_check = builder_->GetInsertBlock();

    builder_->SetInsertPoint(merge_bb);
    llvm::PHINode* result_phi =
        builder_->CreatePHI(opaquePtr(), dispatch_classes.size(), "meth.result");

    for (parser::ClassDecl* decl : dispatch_classes) {
        const std::string& class_name = decl->name.lexeme;
        llvm::BasicBlock* call_bb =
            llvm::BasicBlock::Create(*context_, "meth.call." + class_name, parent_fn);
        llvm::BasicBlock* cont_bb =
            llvm::BasicBlock::Create(*context_, "meth.cont." + class_name, parent_fn);

        builder_->SetInsertPoint(next_check);
        llvm::Value* class_name_ptr =
            builder_->CreateBitCast(getClassNameGlobal(class_name), opaquePtr());
        llvm::Value* matches = builder_->CreateICmpEQ(runtime_type, class_name_ptr);
        builder_->CreateCondBr(matches, call_bb, cont_bb);

        builder_->SetInsertPoint(call_bb);
        const MethodResolution resolution = resolveMethod(decl, method_name);
        llvm::Function* fn = lookupMethod(resolution.declaring_class->name.lexeme, method_name,
                                           args.size());
        if (fn == nullptr) {
            fail("Metodo interno '" + method_name + "' no declarado");
            return;
        }

        std::vector<llvm::Value*> call_args;
        call_args.push_back(instance_ptr);
        call_args.insert(call_args.end(), arg_values.begin(), arg_values.end());
        llvm::Value* call_result = builder_->CreateCall(fn, call_args);
        result_phi->addIncoming(call_result, call_bb);
        builder_->CreateBr(merge_bb);

        next_check = cont_bb;
    }

    builder_->SetInsertPoint(next_check);
    emitCastFailure();

    builder_->SetInsertPoint(merge_bb);
    current_value_ = result_phi;
}

void LLVMCodeGenerator::emitMethodCall(parser::Expr* object_expr, const std::string& method_name,
                                       const std::vector<parser::ExprPtr>& args) {
    llvm::StructType* struct_ty = nullptr;
    llvm::Value* instance_ptr = resolveInstancePtr(object_expr, &struct_ty);
    if (instance_ptr == nullptr || had_error_) {
        return;
    }

    emitMethodCallOnInstance(instance_ptr, struct_ty, method_name, args);
}

llvm::Value* LLVMCodeGenerator::boxValueForStorage(llvm::Value* value) {
    return coerceToLlvmType(value, opaquePtr());
}

void LLVMCodeGenerator::registerValueHelperDeclarations() {
    llvm::PointerType* ptr_ty = opaquePtr();
    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*context_);

    module_->getOrInsertFunction("hulk_box_number",
                                 llvm::FunctionType::get(ptr_ty, {double_ty}, false));
    module_->getOrInsertFunction("hulk_unbox_number",
                                 llvm::FunctionType::get(double_ty, {ptr_ty}, false));
    module_->getOrInsertFunction("hulk_box_bool",
                                 llvm::FunctionType::get(ptr_ty, {i32_ty}, false));
    module_->getOrInsertFunction("hulk_print_instance",
                                 llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), {ptr_ty}, false));
}

void LLVMCodeGenerator::registerPrintDeclarations() {
    llvm::PointerType* ptr_ty = opaquePtr();

    llvm::FunctionType* print_double_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context_), {llvm::Type::getDoubleTy(*context_)}, false);
    module_->getOrInsertFunction("hulk_print_double", print_double_ty);

    llvm::FunctionType* print_bool_ty = llvm::FunctionType::get(
        llvm::Type::getVoidTy(*context_), {llvm::Type::getInt32Ty(*context_)}, false);
    module_->getOrInsertFunction("hulk_print_bool", print_bool_ty);

    module_->getOrInsertFunction("hulk_print_null", llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), false));
    module_->getOrInsertFunction("hulk_print_newline",
                                 llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), false));

    llvm::FunctionType* print_boxed_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), {ptr_ty}, false);
    module_->getOrInsertFunction("hulk_print_boxed", print_boxed_ty);

    llvm::FunctionType* concat_ty = llvm::FunctionType::get(ptr_ty, {ptr_ty, ptr_ty}, false);
    module_->getOrInsertFunction("hulk_string_concat", concat_ty);
    module_->getOrInsertFunction("hulk_string_concat_ws", concat_ty);

    llvm::FunctionType* string_eq_ty =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*context_), {ptr_ty, ptr_ty}, false);
    module_->getOrInsertFunction("hulk_string_equals", string_eq_ty);
    module_->getOrInsertFunction("hulk_boxed_equals", string_eq_ty);
}

void LLVMCodeGenerator::enterScope() {
    auto scope = std::make_unique<LLVMScope>(current_scope_);
    current_scope_ = scope.get();
    scopes_.push_back(std::move(scope));
}

void LLVMCodeGenerator::exitScope() {
    if (current_scope_ != nullptr && current_scope_->parent != nullptr) {
        current_scope_ = current_scope_->parent;
        scopes_.pop_back();
    }
}

bool LLVMCodeGenerator::lookupLocalVariable(const std::string& name, llvm::AllocaInst** out_alloca,
                                            llvm::Type** out_type) {
    for (LLVMScope* scope = current_scope_; scope != nullptr; scope = scope->parent) {
        const auto found = scope->variables.find(name);
        if (found != scope->variables.end()) {
            if (out_alloca != nullptr) {
                *out_alloca = found->second;
            }
            if (out_type != nullptr) {
                const auto type_it = scope->variable_types.find(name);
                if (type_it == scope->variable_types.end()) {
                    return false;
                }
                *out_type = type_it->second;
            }
            return true;
        }
    }
    return false;
}

void LLVMCodeGenerator::registerMathematicalConstants() {
    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);

    llvm::Constant* pi_value = llvm::ConstantFP::get(double_ty, kPi);
    auto* pi_global = new llvm::GlobalVariable(
        *module_, double_ty, true, llvm::GlobalValue::ExternalLinkage, pi_value, "PI");
    pi_global->setAlignment(llvm::MaybeAlign(8));
    global_constants_["PI"] = pi_global;

    llvm::Constant* e_value = llvm::ConstantFP::get(double_ty, kE);
    auto* e_global = new llvm::GlobalVariable(
        *module_, double_ty, true, llvm::GlobalValue::ExternalLinkage, e_value, "E");
    e_global->setAlignment(llvm::MaybeAlign(8));
    global_constants_["E"] = e_global;
}

llvm::StructType* LLVMCodeGenerator::getBoxedValueType() {
    if (boxed_value_type_ != nullptr) {
        return boxed_value_type_;
    }

    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt32Ty(*context_),
        llvm::ArrayType::get(llvm::Type::getInt8Ty(*context_), 8),
    };
    boxed_value_type_ = llvm::StructType::create(*context_, fields, "BoxedValue");
    return boxed_value_type_;
}

llvm::GlobalVariable* LLVMCodeGenerator::registerStringConstant(const std::string& value) {
    const auto found = string_constants_.find(value);
    if (found != string_constants_.end()) {
        return found->second;
    }

    llvm::Constant* str_constant = llvm::ConstantDataArray::getString(*context_, value, true);
    auto* global_var = new llvm::GlobalVariable(
        *module_,
        str_constant->getType(),
        true,
        llvm::GlobalValue::PrivateLinkage,
        str_constant,
        "");
    string_constants_[value] = global_var;
    return global_var;
}

llvm::Value* LLVMCodeGenerator::createBoxedFromDouble(llvm::Value* double_val) {
    llvm::Function* fn = module_->getFunction("hulk_box_number");
    return builder_->CreateCall(fn, {double_val}, "boxed_double");
}

llvm::Value* LLVMCodeGenerator::createBoxedFromBool(llvm::Value* bool_val) {
    llvm::Function* fn = module_->getFunction("hulk_box_bool");
    llvm::Value* as_i32 =
        builder_->CreateZExt(bool_val, llvm::Type::getInt32Ty(*context_), "boxed_bool_i32");
    return builder_->CreateCall(fn, {as_i32}, "boxed_bool");
}

llvm::Value* LLVMCodeGenerator::createBoxedFromString(llvm::Value* str_val) {
    llvm::StructType* boxed_ty = getBoxedValueType();
    llvm::Function* malloc_fn = getMallocFunction(*module_, *context_);

    llvm::Value* size = llvm::ConstantExpr::getSizeOf(boxed_ty);
    llvm::Value* raw_ptr = builder_->CreateCall(malloc_fn, {size});
    llvm::Value* boxed = builder_->CreateBitCast(raw_ptr, opaquePtr(), "boxed_str");

    builder_->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 2),
                          builder_->CreateStructGEP(boxed_ty, boxed, 0));

    llvm::Value* data_ptr = builder_->CreateStructGEP(boxed_ty, boxed, 1);
    builder_->CreateStore(str_val, data_ptr);
    return boxed;
}

void LLVMCodeGenerator::materializeExprResult() {
    if (current_value_ == nullptr) {
        return;
    }

    llvm::Type* value_ty = current_value_->getType();
    llvm::AllocaInst* slot = builder_->CreateAlloca(value_ty, nullptr, "expr_tmp");
    builder_->CreateStore(current_value_, slot);
    current_value_ = nullptr;
}

void LLVMCodeGenerator::createEmptyMain() {
    llvm::FunctionType* main_type =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*context_), false);
    llvm::Function* main_fn = llvm::Function::Create(
        main_type, llvm::Function::ExternalLinkage, "main", module_.get());
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", main_fn);
    builder_->SetInsertPoint(entry);
    builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));
}

void LLVMCodeGenerator::generate(parser::Program* program) {
    if (program == nullptr) {
        createEmptyMain();
        return;
    }
    program->accept(this);
}

std::string LLVMCodeGenerator::getIR() const {
    llvm::SmallString<512> storage;
    llvm::raw_svector_ostream stream(storage);
    module_->print(stream, nullptr);
    return std::string(storage.str());
}

void LLVMCodeGenerator::visit(parser::Program* stmt) {
    llvm::FunctionType* main_type =
        llvm::FunctionType::get(llvm::Type::getInt32Ty(*context_), false);
    llvm::Function* main_fn = llvm::Function::Create(
        main_type, llvm::Function::ExternalLinkage, "main", module_.get());
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", main_fn);
    builder_->SetInsertPoint(entry);

    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::FUNCTION_DECL) {
            declareUserFunction(static_cast<parser::FunctionDecl*>(child.get()));
            if (had_error_) {
                return;
            }
        }
    }

    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::CLASS_DECL) {
            registerClassDecl(static_cast<parser::ClassDecl*>(child.get()));
            if (had_error_) {
                return;
            }
        }
    }

    buildAllClassStructTypes();
    if (had_error_) {
        return;
    }

    std::size_t method_count = 0;
    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::CLASS_DECL) {
            method_count += static_cast<parser::ClassDecl*>(child.get())->methods.size();
        }
    }
    method_mangled_names_.reserve(method_mangled_names_.size() + method_count);

    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::CLASS_DECL) {
            declareClassMethods(static_cast<parser::ClassDecl*>(child.get()));
            if (had_error_) {
                return;
            }
        }
    }

    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::FUNCTION_DECL) {
            defineUserFunction(static_cast<parser::FunctionDecl*>(child.get()));
            if (had_error_) {
                return;
            }
        }
    }

    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::CLASS_DECL) {
            defineClassMethods(static_cast<parser::ClassDecl*>(child.get()));
            if (had_error_) {
                return;
            }
        }
    }

    for (const auto& child : stmt->stmts) {
        if (child->stmt_kind == parser::StmtKind::FUNCTION_DECL ||
            child->stmt_kind == parser::StmtKind::CLASS_DECL) {
            continue;
        }
        child->accept(this);
        if (had_error_) {
            return;
        }
    }

    builder_->CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0));

    llvm::SmallString<256> verify_storage;
    llvm::raw_svector_ostream verify_stream(verify_storage);
    if (llvm::verifyModule(*module_, &verify_stream)) {
        fail("IR invalido: " + std::string(verify_storage.str()));
    }
}

void LLVMCodeGenerator::visit(parser::ExprStmt* stmt) {
    stmt->expr->accept(this);
    if (had_error_) {
        return;
    }
    materializeExprResult();
}

void LLVMCodeGenerator::visit(parser::NumberExpr* expr) {
    const double value = std::stod(expr->token.lexeme);
    current_value_ = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), value);
}

void LLVMCodeGenerator::visit(parser::StringExpr* expr) {
    llvm::GlobalVariable* str_global = registerStringConstant(expr->token.lexeme);
    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0);
    llvm::Value* str_ptr = builder_->CreateInBoundsGEP(
        str_global->getValueType(), str_global, {zero, zero}, "str_ptr");
    current_value_ = createBoxedFromString(str_ptr);
}

llvm::Function* LLVMCodeGenerator::getLibmFunction(const char* name, llvm::FunctionType* fn_type) {
    llvm::Function* fn = module_->getFunction(name);
    if (fn != nullptr) {
        return fn;
    }
    return llvm::Function::Create(fn_type, llvm::Function::ExternalLinkage, name, module_.get());
}

llvm::Value* LLVMCodeGenerator::expectBoolValue(llvm::Value* value, const std::string& op) {
    if (value == nullptr || !value->getType()->isIntegerTy(1)) {
        fail("Operador '" + op + "' requiere operandos booleanos");
        return nullptr;
    }
    return value;
}

llvm::Value* LLVMCodeGenerator::expectDoubleValue(llvm::Value* value, const std::string& op) {
    if (value == nullptr || !value->getType()->isDoubleTy()) {
        fail("Operador '" + op + "' requiere operandos numericos");
        return nullptr;
    }
    return value;
}

llvm::Value* LLVMCodeGenerator::unboxDoubleValue(llvm::Value* value, const std::string& op) {
    if (value == nullptr) {
        fail("codegen: valor nulo en " + op);
        return nullptr;
    }
    if (value->getType()->isDoubleTy()) {
        return value;
    }
    if (isBoxedValue(value)) {
        llvm::StructType* boxed_ty = getBoxedValueType();
        llvm::Value* struct_ptr =
            builder_->CreateBitCast(value, boxed_ty->getPointerTo(), "boxed_num_ptr");
        llvm::Value* data_gep = builder_->CreateStructGEP(boxed_ty, struct_ptr, 1, "boxed_num_data");
        return builder_->CreateLoad(llvm::Type::getDoubleTy(*context_), data_gep, "unboxed_num");
    }
    fail("Operador '" + op + "' requiere operandos numericos");
    return nullptr;
}

llvm::Value* LLVMCodeGenerator::unboxBoolValue(llvm::Value* value, const std::string& op) {
    if (value == nullptr) {
        fail("codegen: valor nulo en " + op);
        return nullptr;
    }
    if (value->getType()->isIntegerTy(1)) {
        return value;
    }
    if (isBoxedValue(value)) {
        llvm::StructType* boxed_ty = getBoxedValueType();
        llvm::Value* struct_ptr =
            builder_->CreateBitCast(value, boxed_ty->getPointerTo(), "boxed_bool_ptr");
        llvm::Value* data_gep = builder_->CreateStructGEP(boxed_ty, struct_ptr, 1, "boxed_bool_data");
        return builder_->CreateLoad(llvm::Type::getInt1Ty(*context_), data_gep, "unboxed_bool");
    }
    fail("Operador '" + op + "' requiere operandos booleanos");
    return nullptr;
}

llvm::Value* LLVMCodeGenerator::coerceStringBoxed(llvm::Value* value, const std::string& op) {
    if (value == nullptr) {
        fail("codegen: valor nulo en " + op);
        return nullptr;
    }
    if (isBoxedValue(value)) {
        return value;
    }
    if (value->getType()->isPointerTy()) {
        return createBoxedFromString(value);
    }
    fail("Operador '" + op + "' requiere operandos string");
    return nullptr;
}

llvm::Value* LLVMCodeGenerator::emitBoxedEquality(llvm::Value* left, llvm::Value* right, bool equal) {
    llvm::Function* fn = module_->getFunction("hulk_boxed_equals");
    llvm::Value* cmp_i32 = builder_->CreateCall(fn, {left, right}, "boxed_eq");
    llvm::Value* cmp_bool = builder_->CreateICmpNE(
        cmp_i32, llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0), "boxed_eq_bool");
    if (equal) {
        return cmp_bool;
    }
    return builder_->CreateNot(cmp_bool, "boxed_ne");
}

llvm::Value* LLVMCodeGenerator::defaultValueForType(llvm::Type* type) {
    if (type == nullptr) {
        return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), 0.0);
    }
    if (type->isDoubleTy()) {
        return llvm::ConstantFP::get(type, 0.0);
    }
    if (type->isIntegerTy(1)) {
        return llvm::ConstantInt::getFalse(*context_);
    }
    if (type->isPointerTy()) {
        return llvm::ConstantPointerNull::get(opaquePtr());
    }
    return llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), 0.0);
}

void LLVMCodeGenerator::emitLogicalAndShortCircuit(parser::Expr* right_expr) {
    llvm::Value* left_val = current_value_;
    if (expectBoolValue(left_val, "&&") == nullptr) {
        return;
    }

    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* lhs_bb = builder_->GetInsertBlock();
    llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*context_, "land.rhs", fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "land.end", fn);

    builder_->CreateCondBr(left_val, rhs_bb, merge_bb);

    builder_->SetInsertPoint(rhs_bb);
    right_expr->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* right_val = expectBoolValue(current_value_, "&&");
    if (right_val == nullptr) {
        return;
    }
    builder_->CreateBr(merge_bb);

    builder_->SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt1Ty(*context_), 2, "land");
    phi->addIncoming(llvm::ConstantInt::getFalse(*context_), lhs_bb);
    phi->addIncoming(right_val, rhs_bb);
    current_value_ = phi;
}

void LLVMCodeGenerator::emitLogicalOrShortCircuit(parser::Expr* right_expr) {
    llvm::Value* left_val = current_value_;
    if (expectBoolValue(left_val, "||") == nullptr) {
        return;
    }

    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* lhs_bb = builder_->GetInsertBlock();
    llvm::BasicBlock* rhs_bb = llvm::BasicBlock::Create(*context_, "lor.rhs", fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "lor.end", fn);

    builder_->CreateCondBr(left_val, merge_bb, rhs_bb);

    builder_->SetInsertPoint(rhs_bb);
    right_expr->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* right_val = expectBoolValue(current_value_, "||");
    if (right_val == nullptr) {
        return;
    }
    builder_->CreateBr(merge_bb);

    builder_->SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt1Ty(*context_), 2, "lor");
    phi->addIncoming(llvm::ConstantInt::getTrue(*context_), lhs_bb);
    phi->addIncoming(right_val, rhs_bb);
    current_value_ = phi;
}

void LLVMCodeGenerator::visit(parser::BoolExpr* expr) {
    current_value_ = llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), expr->value ? 1 : 0);
}

void LLVMCodeGenerator::visit(parser::NullExpr* /*expr*/) {
    current_value_ = llvm::ConstantPointerNull::get(opaquePtr());
}

void LLVMCodeGenerator::visit(parser::IdentifierExpr* expr) {
    const std::string& name = expr->token.lexeme;

    const auto constant = global_constants_.find(name);
    if (constant != global_constants_.end()) {
        llvm::GlobalVariable* global = constant->second;
        current_value_ = builder_->CreateLoad(global->getValueType(), global);
        return;
    }

    llvm::AllocaInst* alloca = nullptr;
    llvm::Type* var_type = nullptr;
    if (lookupLocalVariable(name, &alloca, &var_type)) {
        llvm::Type* load_ty = var_type;
        if (isBoxedSemanticType(var_type) || isRangeSemanticType(var_type) || isInstanceSemanticType(var_type) ||
            var_type == opaquePtr()) {
            load_ty = opaquePtr();
        }
        current_value_ = builder_->CreateLoad(load_ty, alloca);
        return;
    }

    if (current_self_alloca_ != nullptr && current_self_class_ != nullptr) {
        ClassInfo* info = lookupClassInfo(current_self_class_->name.lexeme);
        if (info != nullptr && info->field_index.find(name) != info->field_index.end()) {
            llvm::Value* self_ptr = builder_->CreateLoad(opaquePtr(), current_self_alloca_);
            current_value_ =
                loadInstanceField(self_ptr, info->struct_ty, current_self_class_, name);
            return;
        }
    }

    fail("codegen: variable '" + name + "' no definida");
}

void LLVMCodeGenerator::visit(parser::LetExpr* expr) {
    expr->initializer->accept(this);
    if (had_error_) {
        return;
    }

    llvm::Value* init_value = current_value_;
    llvm::Type* var_type = nullptr;
    if (expr->declared_type.has_value()) {
        const std::string& declared = expr->declared_type->lexeme;
        if (class_decls_.find(declared) != class_decls_.end()) {
            var_type = getInstanceStructType(declared);
            if (var_type == nullptr) {
                fail("codegen: tipo de instancia no registrado para let");
                return;
            }
        }
    }
    if (var_type == nullptr && expr->initializer->kind == parser::ExprKind::NEW_OBJ) {
        const auto* new_expr = static_cast<const parser::NewExpr*>(expr->initializer.get());
        var_type = getInstanceStructType(new_expr->type_name.lexeme);
        if (var_type == nullptr) {
            fail("codegen: tipo de instancia no registrado para let");
            return;
        }
    }
    if (var_type == nullptr) {
        var_type = classifySemanticType(init_value);
    }
    llvm::Type* storage_ty = init_value->getType();
    if (init_value->getType()->isPointerTy()) {
        storage_ty = opaquePtr();
    }
    const std::string var_name = expr->name.lexeme;

    enterScope();

    llvm::AllocaInst* alloca = builder_->CreateAlloca(storage_ty, nullptr, "");
    builder_->CreateStore(init_value, alloca);
    current_scope_->variables[var_name] = alloca;
    current_scope_->variable_types[var_name] = var_type;

    expr->body->accept(this);

    exitScope();
}

llvm::StructType* LLVMCodeGenerator::getRangeType() {
    if (range_type_ != nullptr) {
        return range_type_;
    }

    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    range_type_ = llvm::StructType::create(*context_, {double_ty, double_ty}, "HulkRange");
    return range_type_;
}

llvm::PointerType* LLVMCodeGenerator::opaquePtr() {
    return llvm::PointerType::getUnqual(*context_);
}

bool LLVMCodeGenerator::lookupSemanticTypeForAlloca(llvm::AllocaInst* alloca,
                                                    llvm::Type** out_type) {
    if (alloca == nullptr) {
        return false;
    }
    for (LLVMScope* scope = current_scope_; scope != nullptr; scope = scope->parent) {
        for (const auto& [name, slot] : scope->variables) {
            if (slot == alloca) {
                if (out_type != nullptr) {
                    const auto type_it = scope->variable_types.find(name);
                    if (type_it == scope->variable_types.end()) {
                        return false;
                    }
                    *out_type = type_it->second;
                }
                return true;
            }
        }
    }
    return false;
}

llvm::Type* LLVMCodeGenerator::classifySemanticType(llvm::Value* value) {
    if (value == nullptr) {
        return llvm::Type::getDoubleTy(*context_);
    }

    llvm::Type* llvm_ty = value->getType();
    if (!llvm_ty->isPointerTy()) {
        return llvm_ty;
    }

    if (llvm::isa<llvm::ConstantPointerNull>(value)) {
        return opaquePtr();
    }

    if (auto* call = llvm::dyn_cast<llvm::CallInst>(value)) {
        llvm::Function* callee = call->getCalledFunction();
        if (callee != nullptr) {
            const llvm::StringRef name = callee->getName();
            if (name == "hulk_range_create") {
                return getRangeType();
            }
            if (name == "hulk_string_concat" || name == "hulk_string_concat_ws") {
                return getBoxedValueType();
            }
        }
    }

    if (auto it = value_instance_types_.find(value); it != value_instance_types_.end()) {
        return it->second;
    }

    if (auto* cast = llvm::dyn_cast<llvm::BitCastInst>(value)) {
        if (cast->hasName() && cast->getName().starts_with("boxed_")) {
            return getBoxedValueType();
        }
        if (cast->hasName() && cast->getName().starts_with("inst_")) {
            const std::string class_name = cast->getName().substr(5).str();
            llvm::StructType* struct_ty = getInstanceStructType(class_name);
            if (struct_ty != nullptr) {
                return struct_ty;
            }
        }
    }

    if (auto* load = llvm::dyn_cast<llvm::LoadInst>(value)) {
        llvm::Type* sem_type = nullptr;
        if (auto* alloca = llvm::dyn_cast<llvm::AllocaInst>(load->getPointerOperand())) {
            if (lookupSemanticTypeForAlloca(alloca, &sem_type)) {
                return sem_type;
            }
        }
    }

    return getBoxedValueType();
}

bool LLVMCodeGenerator::isBoxedSemanticType(llvm::Type* type) {
    return type == getBoxedValueType();
}

bool LLVMCodeGenerator::isRangeSemanticType(llvm::Type* type) {
    return type == getRangeType();
}

bool LLVMCodeGenerator::isRangeValue(llvm::Value* value) {
    return classifySemanticType(value) == getRangeType();
}

bool LLVMCodeGenerator::isBoxedValue(llvm::Value* value) {
    if (value == nullptr || !value->getType()->isPointerTy()) {
        return false;
    }
    if (llvm::isa<llvm::ConstantPointerNull>(value)) {
        return false;
    }
    return isBoxedSemanticType(classifySemanticType(value));
}

bool LLVMCodeGenerator::isRangePointer(llvm::Type* type) {
    return isRangeSemanticType(type);
}

bool LLVMCodeGenerator::isBoxedValuePointer(llvm::Type* type) {
    return isBoxedSemanticType(type);
}

bool LLVMCodeGenerator::emitBuiltinCall(const std::string& name,
                                        const std::vector<parser::ExprPtr>& args) {
    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);

    if (name == "sin" || name == "cos" || name == "sqrt" || name == "log" || name == "exp") {
        if (args.size() != 1) {
            fail(name + " espera 1 argumento");
            return true;
        }
        args[0]->accept(this);
        if (had_error_) {
            return true;
        }
        llvm::Value* x = expectDoubleValue(current_value_, name);
        if (x == nullptr) {
            return true;
        }
        llvm::FunctionType* fn_ty = llvm::FunctionType::get(double_ty, {double_ty}, false);
        llvm::Function* fn = getLibmFunction(name.c_str(), fn_ty);
        current_value_ = builder_->CreateCall(fn, {x});
        return true;
    }

    if (name == "rand") {
        if (!args.empty()) {
            fail("rand no acepta argumentos");
            return true;
        }
        llvm::Function* rand_fn = module_->getFunction("rand");
        llvm::Value* raw = builder_->CreateCall(rand_fn, {});
        llvm::Value* as_double = builder_->CreateUIToFP(raw, double_ty);
        llvm::Value* rand_max = llvm::ConstantFP::get(double_ty, 32767.0);
        current_value_ = builder_->CreateFDiv(as_double, rand_max);
        return true;
    }

    if (name == "range") {
        if (args.size() != 2) {
            fail("range espera 2 argumentos");
            return true;
        }
        args[0]->accept(this);
        if (had_error_) {
            return true;
        }
        llvm::Value* start = expectDoubleValue(current_value_, name);
        if (start == nullptr) {
            return true;
        }
        args[1]->accept(this);
        if (had_error_) {
            return true;
        }
        llvm::Value* end = expectDoubleValue(current_value_, name);
        if (end == nullptr) {
            return true;
        }
        llvm::Function* create_fn = module_->getFunction("hulk_range_create");
        current_value_ = builder_->CreateCall(create_fn, {start, end});
        return true;
    }

    return false;
}

void LLVMCodeGenerator::emitPrintNewline() {
    llvm::Function* fn = module_->getFunction("hulk_print_newline");
    builder_->CreateCall(fn, {});
}

void LLVMCodeGenerator::emitPrintValue(llvm::Value* value) {
    if (value == nullptr) {
        fail("codegen: valor nulo en print");
        return;
    }

    if (value->getType()->isDoubleTy()) {
        llvm::Function* fn = module_->getFunction("hulk_print_double");
        builder_->CreateCall(fn, {value});
        return;
    }

    if (value->getType()->isIntegerTy(1)) {
        llvm::Function* fn = module_->getFunction("hulk_print_bool");
        llvm::Value* as_i32 = builder_->CreateZExt(value, llvm::Type::getInt32Ty(*context_), "print_bool_i32");
        builder_->CreateCall(fn, {as_i32});
        return;
    }

    if (!value->getType()->isPointerTy()) {
        fail("codegen: tipo no imprimible en print");
        return;
    }

    if (isBoxedValue(value)) {
        llvm::Function* fn = module_->getFunction("hulk_print_boxed");
        builder_->CreateCall(fn, {value});
        return;
    }

    if (isInstanceValue(value)) {
        llvm::Function* fn = module_->getFunction("hulk_print_instance");
        builder_->CreateCall(fn, {value});
        return;
    }

    llvm::Function* null_fn = module_->getFunction("hulk_print_null");
    builder_->CreateCall(null_fn, {});
}

void LLVMCodeGenerator::visit(parser::CallExpr* expr) {
    if (expr->callee->kind == parser::ExprKind::GET_ATTR) {
        const auto* get_attr = static_cast<const parser::GetAttrExpr*>(expr->callee.get());
        emitMethodCall(get_attr->object.get(), get_attr->name.lexeme, expr->args);
        return;
    }

    if (expr->callee->kind != parser::ExprKind::IDENTIFIER) {
        fail("codegen: llamada no soportada");
        return;
    }

    const auto* callee = static_cast<parser::IdentifierExpr*>(expr->callee.get());
    const std::string& name = callee->token.lexeme;

    if (name == "print") {
        if (expr->args.empty()) {
            emitPrintNewline();
            current_value_ = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), 0.0);
            return;
        }

        if (expr->args.size() != 1) {
            fail("print espera 1 argumento");
            return;
        }

        expr->args[0]->accept(this);
        if (had_error_) {
            return;
        }
        emitPrintValue(current_value_);
        current_value_ = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), 0.0);
        return;
    }

    if (isBuiltinFunctionName(name)) {
        if (emitBuiltinCall(name, expr->args)) {
            return;
        }
        fail("codegen: builtin '" + name + "' no implementado");
        return;
    }

    const std::size_t arity = expr->args.size();
    llvm::Function* fn = lookupUserFunction(name, arity);
    if (fn == nullptr) {
        fail("La funcion '" + name + "' no esta definida con aridad " + std::to_string(arity));
        return;
    }

    const std::shared_ptr<FunctionSymbol> sym = lookupUserFunctionSymbol(name, arity);
    std::vector<TypeInfo> param_types;
    if (sym) {
        param_types = sym->parameter_types;
    } else {
        param_types.assign(arity, TypeInfo::Number());
    }
    const TypeInfo return_type = sym ? sym->return_type : TypeInfo::Number();

    std::vector<llvm::Value*> arg_values;
    arg_values.reserve(arity);
    for (std::size_t i = 0; i < arity; ++i) {
        expr->args[i]->accept(this);
        if (had_error_) {
            return;
        }
        const TypeInfo param_info = i < param_types.size() ? param_types[i] : TypeInfo::Number();
        llvm::Type* param_ty = llvmTypeForTypeInfo(param_info);
        llvm::Value* value = coerceToLlvmType(current_value_, param_ty);
        if (value == nullptr) {
            return;
        }
        arg_values.push_back(value);
    }

    llvm::Value* call_result = invokeUserFunction(fn, arg_values);
    current_value_ = demoteFromCallResult(call_result, return_type);
}

void LLVMCodeGenerator::visit(parser::BinaryExpr* expr) {
    const std::string& op = expr->op.lexeme;

    if (isLogicalAndOp(op)) {
        expr->left->accept(this);
        if (had_error_) {
            return;
        }
        emitLogicalAndShortCircuit(expr->right.get());
        return;
    }

    if (isLogicalOrOp(op)) {
        expr->left->accept(this);
        if (had_error_) {
            return;
        }
        emitLogicalOrShortCircuit(expr->right.get());
        return;
    }

    expr->left->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* left = current_value_;

    expr->right->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* right = current_value_;

    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);

    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%" || op == "^") {
        left = expectDoubleValue(left, op);
        right = expectDoubleValue(right, op);
        if (left == nullptr || right == nullptr) {
            return;
        }

        if (op == "+") {
            current_value_ = builder_->CreateFAdd(left, right, "add");
        } else if (op == "-") {
            current_value_ = builder_->CreateFSub(left, right, "sub");
        } else if (op == "*") {
            current_value_ = builder_->CreateFMul(left, right, "mul");
        } else if (op == "/") {
            current_value_ = builder_->CreateFDiv(left, right, "div");
        } else if (op == "%") {
            llvm::FunctionType* fmod_ty = llvm::FunctionType::get(double_ty, {double_ty, double_ty}, false);
            llvm::Function* fmod_fn = getLibmFunction("fmod", fmod_ty);
            current_value_ = builder_->CreateCall(fmod_fn, {left, right}, "mod");
        } else {
            llvm::FunctionType* pow_ty = llvm::FunctionType::get(double_ty, {double_ty, double_ty}, false);
            llvm::Function* pow_fn = getLibmFunction("pow", pow_ty);
            current_value_ = builder_->CreateCall(pow_fn, {left, right}, "pow");
        }
        return;
    }

    if (op == "<" || op == "<=" || op == ">" || op == ">=" || op == "==" || op == "!=") {
        if (op == "==" || op == "!=") {
            if (left->getType()->isIntegerTy(1) && right->getType()->isIntegerTy(1)) {
                if (op == "==") {
                    current_value_ = builder_->CreateICmpEQ(left, right, "cmp");
                } else {
                    current_value_ = builder_->CreateICmpNE(left, right, "cmp");
                }
                return;
            }

            if (isBoxedValue(left) && isBoxedValue(right)) {
                current_value_ = emitBoxedEquality(left, right, op == "==");
                return;
            }

            const bool left_numeric =
                left->getType()->isDoubleTy() || (left->getType()->isPointerTy() && isBoxedValue(left));
            const bool right_numeric =
                right->getType()->isDoubleTy() || (right->getType()->isPointerTy() && isBoxedValue(right));
            if (left_numeric && right_numeric) {
                left = unboxDoubleValue(left, op);
                right = unboxDoubleValue(right, op);
                if (left == nullptr || right == nullptr) {
                    return;
                }
                if (op == "==") {
                    current_value_ = builder_->CreateFCmpOEQ(left, right, "cmp");
                } else {
                    current_value_ = builder_->CreateFCmpONE(left, right, "cmp");
                }
                return;
            }

            if (left->getType()->isPointerTy() && right->getType()->isPointerTy() &&
                !isInstanceValue(left) && !isInstanceValue(right) &&
                (!isBoxedValue(left) || !isBoxedValue(right))) {
                const llvm::CmpInst::Predicate pred =
                    op == "==" ? llvm::CmpInst::ICMP_EQ : llvm::CmpInst::ICMP_NE;
                current_value_ = builder_->CreateICmp(pred, left, right, "cmp");
                return;
            }

            fail("Operador binario no soportado: " + op);
            return;
        }

        const bool left_numeric =
            left->getType()->isDoubleTy() || (left->getType()->isPointerTy() && isBoxedValue(left));
        const bool right_numeric =
            right->getType()->isDoubleTy() || (right->getType()->isPointerTy() && isBoxedValue(right));
        if (left_numeric && right_numeric) {
            left = unboxDoubleValue(left, op);
            right = unboxDoubleValue(right, op);
            if (left == nullptr || right == nullptr) {
                return;
            }

            llvm::CmpInst::Predicate pred = llvm::CmpInst::FCMP_OLT;
            if (op == "<=") {
                pred = llvm::CmpInst::FCMP_OLE;
            } else if (op == ">") {
                pred = llvm::CmpInst::FCMP_OGT;
            } else if (op == ">=") {
                pred = llvm::CmpInst::FCMP_OGE;
            }
            current_value_ = builder_->CreateFCmp(pred, left, right, "cmp");
            return;
        }

        fail("Operador binario no soportado: " + op);
        return;
    }

    if (op == "@" || op == "@@") {
        left = coerceStringBoxed(left, op);
        right = coerceStringBoxed(right, op);
        if (left == nullptr || right == nullptr) {
            return;
        }
        const char* fn_name = op == "@" ? "hulk_string_concat" : "hulk_string_concat_ws";
        llvm::Function* fn = module_->getFunction(fn_name);
        current_value_ = builder_->CreateCall(fn, {left, right}, "concat");
        return;
    }

    fail("Operador binario no soportado: " + op);
}

void LLVMCodeGenerator::visit(parser::BlockExpr* expr) {
    for (const auto& sub : expr->exprs) {
        sub->accept(this);
        if (had_error_) {
            return;
        }
    }
}

void LLVMCodeGenerator::visit(parser::IfExpr* expr) {
    llvm::BasicBlock* cond_bb = builder_->GetInsertBlock();

    expr->condition->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* cond = expectBoolValue(current_value_, "if");
    if (cond == nullptr) {
        return;
    }

    llvm::Function* fn = cond_bb->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context_, "if.then", fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "if.end", fn);
    llvm::BasicBlock* else_bb = nullptr;

    if (expr->else_branch) {
        else_bb = llvm::BasicBlock::Create(*context_, "if.else", fn);
        builder_->CreateCondBr(cond, then_bb, else_bb);
    } else {
        builder_->CreateCondBr(cond, then_bb, merge_bb);
    }

    builder_->SetInsertPoint(then_bb);
    expr->then_branch->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* then_val = current_value_;
    if (then_val == nullptr) {
        fail("codegen: rama 'then' de if sin valor");
        return;
    }
    llvm::Type* result_ty = then_val->getType();
    builder_->CreateBr(merge_bb);
    llvm::BasicBlock* then_end = builder_->GetInsertBlock();

    llvm::BasicBlock* else_end = nullptr;
    llvm::Value* else_val = nullptr;
    if (expr->else_branch) {
        builder_->SetInsertPoint(else_bb);
        expr->else_branch->accept(this);
        if (had_error_) {
            return;
        }
        else_val = current_value_;
        if (else_val == nullptr) {
            fail("codegen: rama 'else' de if sin valor");
            return;
        }
        builder_->CreateBr(merge_bb);
        else_end = builder_->GetInsertBlock();
    }

    builder_->SetInsertPoint(merge_bb);
    if (expr->else_branch) {
        llvm::PHINode* phi = builder_->CreatePHI(result_ty, 2, "if.result");
        phi->addIncoming(then_val, then_end);
        phi->addIncoming(else_val, else_end);
        current_value_ = phi;
    } else {
        llvm::PHINode* phi = builder_->CreatePHI(result_ty, 2, "if.result");
        phi->addIncoming(then_val, then_end);
        phi->addIncoming(defaultValueForType(result_ty), cond_bb);
        current_value_ = phi;
    }
}

void LLVMCodeGenerator::visit(parser::WhileExpr* expr) {
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context_, "while.cond", fn);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "while.body", fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*context_, "while.end", fn);

    if (expr->else_branch) {
        llvm::BasicBlock* else_bb = llvm::BasicBlock::Create(*context_, "while.else", fn);

        builder_->CreateBr(cond_bb);

        builder_->SetInsertPoint(cond_bb);
        expr->condition->accept(this);
        if (had_error_) {
            return;
        }
        llvm::Value* cond = expectBoolValue(current_value_, "while");
        if (cond == nullptr) {
            return;
        }
        builder_->CreateCondBr(cond, body_bb, else_bb);

        builder_->SetInsertPoint(body_bb);
        expr->body->accept(this);
        if (had_error_) {
            return;
        }
        builder_->CreateBr(cond_bb);

        builder_->SetInsertPoint(else_bb);
        expr->else_branch->accept(this);
        if (had_error_) {
            return;
        }
        builder_->CreateBr(end_bb);

        builder_->SetInsertPoint(end_bb);
        return;
    }

    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    llvm::Type* i1_ty = llvm::Type::getInt1Ty(*context_);
    llvm::AllocaInst* result_alloca = builder_->CreateAlloca(double_ty, nullptr, "while.last");
    llvm::AllocaInst* ran_alloca = builder_->CreateAlloca(i1_ty, nullptr, "while.ran");
    builder_->CreateStore(llvm::ConstantFP::get(double_ty, 0.0), result_alloca);
    builder_->CreateStore(llvm::ConstantInt::getFalse(*context_), ran_alloca);

    builder_->CreateBr(cond_bb);

    builder_->SetInsertPoint(cond_bb);
    expr->condition->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* cond = expectBoolValue(current_value_, "while");
    if (cond == nullptr) {
        return;
    }
    builder_->CreateCondBr(cond, body_bb, end_bb);

    builder_->SetInsertPoint(body_bb);
    expr->body->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* body_val = expectDoubleValue(current_value_, "while");
    if (body_val == nullptr) {
        return;
    }
    builder_->CreateStore(body_val, result_alloca);
    builder_->CreateStore(llvm::ConstantInt::getTrue(*context_), ran_alloca);
    builder_->CreateBr(cond_bb);

    builder_->SetInsertPoint(end_bb);
    llvm::Value* ran = builder_->CreateLoad(i1_ty, ran_alloca);
    llvm::Value* last = builder_->CreateLoad(double_ty, result_alloca);
    current_value_ = builder_->CreateSelect(ran, last, llvm::ConstantFP::get(double_ty, 0.0), "while.result");
}

void LLVMCodeGenerator::visit(parser::AssignExpr* expr) {
    expr->rhs->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* rhs_value = current_value_;
    if (rhs_value == nullptr) {
        fail("codegen: lado derecho de ':=' sin valor");
        return;
    }

    if (expr->lhs->kind != parser::ExprKind::IDENTIFIER) {
        if (expr->lhs->kind == parser::ExprKind::GET_ATTR) {
            const auto* get_attr = static_cast<const parser::GetAttrExpr*>(expr->lhs.get());
            llvm::StructType* struct_ty = nullptr;
            llvm::Value* instance_ptr = resolveInstancePtr(get_attr->object.get(), &struct_ty);
            if (instance_ptr == nullptr || had_error_) {
                return;
            }

            ClassInfo* info = lookupClassInfoByStruct(struct_ty);
            if (info == nullptr || info->decl == nullptr) {
                fail("codegen: tipo de instancia desconocido para asignacion");
                return;
            }

            unsigned idx = 0;
            if (!lookupFieldIndex(info, get_attr->name.lexeme, &idx)) {
                return;
            }
            llvm::Type* field_ty = info->field_llvm_types[idx];
            rhs_value = expectFieldValue(rhs_value, field_ty, ":=");
            if (rhs_value == nullptr) {
                return;
            }
            storeInstanceField(instance_ptr, struct_ty, info->decl, get_attr->name.lexeme, rhs_value);
            current_value_ = rhs_value;
            return;
        }

        fail("codegen: asignacion ':=' no soportada para este destino");
        return;
    }

    const auto* id = static_cast<parser::IdentifierExpr*>(expr->lhs.get());

    const std::string& name = id->token.lexeme;
    llvm::AllocaInst* alloca = nullptr;
    if (!lookupLocalVariable(name, &alloca, nullptr)) {
        fail("codegen: variable '" + name + "' no definida para asignacion");
        return;
    }

    llvm::Type* storage_ty = alloca->getAllocatedType();
    rhs_value = coerceToLlvmType(rhs_value, storage_ty);
    if (rhs_value == nullptr) {
        fail("codegen: tipo incompatible en asignacion a '" + name + "'");
        return;
    }

    builder_->CreateStore(rhs_value, alloca);
    current_value_ = rhs_value;
}

void LLVMCodeGenerator::visit(parser::ForExpr* expr) {
    expr->iterable->accept(this);
    if (had_error_) {
        return;
    }

    if (isRangeValue(current_value_)) {
        llvm::Value* range_ptr = current_value_;

        llvm::StructType* range_ty = getRangeType();
        llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
        llvm::Value* start =
            builder_->CreateLoad(double_ty, builder_->CreateStructGEP(range_ty, range_ptr, 0));
        llvm::Value* end = builder_->CreateLoad(double_ty, builder_->CreateStructGEP(range_ty, range_ptr, 1));

        llvm::Function* parent_fn = builder_->GetInsertBlock()->getParent();
        llvm::BasicBlock* loop_cond = llvm::BasicBlock::Create(*context_, "for.cond", parent_fn);
        llvm::BasicBlock* loop_body = llvm::BasicBlock::Create(*context_, "for.body", parent_fn);
        llvm::BasicBlock* loop_step = llvm::BasicBlock::Create(*context_, "for.step", parent_fn);
        llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*context_, "for.end", parent_fn);

        llvm::AllocaInst* index_slot = builder_->CreateAlloca(double_ty, nullptr, "");
        builder_->CreateStore(start, index_slot);
        builder_->CreateBr(loop_cond);

        builder_->SetInsertPoint(loop_cond);
        llvm::Value* index_val = builder_->CreateLoad(double_ty, index_slot);
        llvm::Value* continue_loop = builder_->CreateFCmpOLT(index_val, end);
        builder_->CreateCondBr(continue_loop, loop_body, loop_end);

        builder_->SetInsertPoint(loop_body);
        enterScope();
        llvm::AllocaInst* var_alloca = builder_->CreateAlloca(double_ty, nullptr, "");
        builder_->CreateStore(index_val, var_alloca);
        current_scope_->variables[expr->variable.lexeme] = var_alloca;
        current_scope_->variable_types[expr->variable.lexeme] = double_ty;

        expr->body->accept(this);
        if (had_error_) {
            exitScope();
            return;
        }
        llvm::Value* body_value = current_value_;
        exitScope();

        builder_->CreateBr(loop_step);

        builder_->SetInsertPoint(loop_step);
        llvm::Value* next_index =
            builder_->CreateFAdd(builder_->CreateLoad(double_ty, index_slot),
                                llvm::ConstantFP::get(double_ty, 1.0));
        builder_->CreateStore(next_index, index_slot);
        builder_->CreateBr(loop_cond);

        builder_->SetInsertPoint(loop_end);
        if (body_value != nullptr) {
            current_value_ = body_value;
        } else {
            current_value_ = llvm::ConstantFP::get(double_ty, 0.0);
        }
        return;
    }

    if (!isInstanceValue(current_value_)) {
        fail("for requiere un iterable (p. ej. range o objeto con next() y current())");
        return;
    }

    llvm::StructType* struct_ty = resolveInstanceStructType(current_value_);
    if (struct_ty == nullptr) {
        fail("for requiere un iterable (objeto con next() y current())");
        return;
    }

    ClassInfo* info = lookupClassInfoByStruct(struct_ty);
    if (info == nullptr || info->decl == nullptr) {
        fail("codegen: tipo de instancia desconocido en for");
        return;
    }

    const MethodResolution next_res = resolveMethod(info->decl, "next");
    const MethodResolution current_res = resolveMethod(info->decl, "current");
    if (next_res.method == nullptr || current_res.method == nullptr) {
        fail("for requiere un iterable (objeto con next() y current())");
        return;
    }
    if (next_res.method->params.size() != 0 || current_res.method->params.size() != 0) {
        fail("next() y current() no deben recibir argumentos");
        return;
    }

    std::string element_type_name = "Number";
    if (expr->declared_type.has_value()) {
        element_type_name = expr->declared_type->lexeme;
    } else if (current_res.method->return_type.has_value()) {
        element_type_name = current_res.method->return_type->lexeme;
    }

    llvm::Type* loop_storage_ty = opaquePtr();
    llvm::Type* loop_semantic_ty = getBoxedValueType();
    if (element_type_name == "Number") {
        loop_storage_ty = llvm::Type::getDoubleTy(*context_);
        loop_semantic_ty = llvm::Type::getDoubleTy(*context_);
    } else if (element_type_name == "Boolean") {
        loop_storage_ty = llvm::Type::getInt1Ty(*context_);
        loop_semantic_ty = llvm::Type::getInt1Ty(*context_);
    } else if (element_type_name == "String") {
        loop_storage_ty = opaquePtr();
        loop_semantic_ty = getBoxedValueType();
    } else if (llvm::StructType* inst_ty = getInstanceStructType(element_type_name)) {
        loop_storage_ty = opaquePtr();
        loop_semantic_ty = inst_ty;
    }

    llvm::AllocaInst* iter_slot = builder_->CreateAlloca(opaquePtr(), nullptr, "for.iter");
    builder_->CreateStore(current_value_, iter_slot);

    llvm::Function* parent_fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* loop_next = llvm::BasicBlock::Create(*context_, "for.next", parent_fn);
    llvm::BasicBlock* loop_body = llvm::BasicBlock::Create(*context_, "for.body", parent_fn);
    llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*context_, "for.end", parent_fn);

    builder_->CreateBr(loop_next);

    builder_->SetInsertPoint(loop_next);
    llvm::Value* iter_ptr = builder_->CreateLoad(opaquePtr(), iter_slot);
    trackInstanceType(iter_ptr, struct_ty);
    emitMethodCallOnInstance(iter_ptr, struct_ty, "next", {});
    if (had_error_) {
        return;
    }
    llvm::Value* has_next = unboxBoolValue(current_value_, "for");
    if (has_next == nullptr) {
        return;
    }
    builder_->CreateCondBr(has_next, loop_body, loop_end);

    builder_->SetInsertPoint(loop_body);
    iter_ptr = builder_->CreateLoad(opaquePtr(), iter_slot);
    trackInstanceType(iter_ptr, struct_ty);
    emitMethodCallOnInstance(iter_ptr, struct_ty, "current", {});
    if (had_error_) {
        return;
    }

    llvm::Value* element_value = current_value_;
    if (element_type_name == "Number") {
        element_value = unboxDoubleValue(current_value_, "for");
    } else if (element_type_name == "Boolean") {
        element_value = unboxBoolValue(current_value_, "for");
    }
    if (element_value == nullptr) {
        return;
    }

    enterScope();
    llvm::AllocaInst* var_alloca = builder_->CreateAlloca(loop_storage_ty, nullptr, "");
    builder_->CreateStore(element_value, var_alloca);
    current_scope_->variables[expr->variable.lexeme] = var_alloca;
    current_scope_->variable_types[expr->variable.lexeme] = loop_semantic_ty;

    expr->body->accept(this);
    if (had_error_) {
        exitScope();
        return;
    }
    llvm::Value* body_value = current_value_;
    exitScope();

    builder_->CreateBr(loop_next);

    builder_->SetInsertPoint(loop_end);
    if (body_value != nullptr) {
        current_value_ = body_value;
    } else {
        current_value_ = llvm::ConstantFP::get(llvm::Type::getDoubleTy(*context_), 0.0);
    }
}

void LLVMCodeGenerator::visit(parser::FunctionDecl* /*stmt*/) {}

void LLVMCodeGenerator::visit(parser::ClassDecl* /*stmt*/) {}

void LLVMCodeGenerator::visit(parser::MethodDecl* /*stmt*/) {}

void LLVMCodeGenerator::visit(parser::AttributeDecl* /*stmt*/) {}

void LLVMCodeGenerator::visit(parser::SelfExpr* /*expr*/) {
    if (current_self_alloca_ == nullptr) {
        fail("codegen: self fuera de metodo");
        return;
    }
    current_value_ = builder_->CreateLoad(opaquePtr(), current_self_alloca_);
}

void LLVMCodeGenerator::initializeParentAttributes(llvm::Value* instance_ptr,
                                                   llvm::StructType* struct_ty,
                                                   parser::ClassDecl* type_def) {
    if (type_def == nullptr || !type_def->parent_name.has_value()) {
        return;
    }

    parser::ClassDecl* parent_decl = getParentClass(type_def);
    if (parent_decl == nullptr) {
        fail("Tipo base no registrado: " + type_def->parent_name->lexeme);
        return;
    }

    initializeParentAttributes(instance_ptr, struct_ty, parent_decl);
    if (had_error_) {
        return;
    }

    if (parent_decl->params.size() != type_def->parent_args.size()) {
        fail("Numero incorrecto de argumentos en llamada a base: " + parent_decl->name.lexeme);
        return;
    }

    ClassInfo* layout_info = lookupClassInfo(type_def->name.lexeme);
    if (layout_info == nullptr) {
        fail("codegen: layout de instancia no registrado");
        return;
    }

    llvm::AllocaInst* saved_self_alloca = current_self_alloca_;
    parser::ClassDecl* saved_self_class = current_self_class_;
    current_self_alloca_ = builder_->CreateAlloca(opaquePtr(), nullptr, "");
    builder_->CreateStore(instance_ptr, current_self_alloca_);
    current_self_class_ = type_def;

    for (std::size_t i = 0; i < parent_decl->params.size(); ++i) {
        const std::string& param_name = parent_decl->params[i].first.lexeme;
        unsigned idx = 0;
        if (!lookupFieldIndex(layout_info, param_name, &idx)) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        llvm::Type* field_ty = layout_info->field_llvm_types[idx];

        type_def->parent_args[i]->accept(this);
        if (had_error_) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        llvm::Value* value = expectFieldValue(current_value_, field_ty, "base arg");
        if (value == nullptr) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        storeInstanceField(instance_ptr, struct_ty, type_def, param_name, value);
    }

    for (const auto& attr : parent_decl->attributes) {
        attr.value->accept(this);
        if (had_error_) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        unsigned idx = 0;
        if (!lookupFieldIndex(layout_info, attr.name.lexeme, &idx)) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        llvm::Type* field_ty = layout_info->field_llvm_types[idx];
        llvm::Value* value = expectFieldValue(current_value_, field_ty, "base attr");
        if (value == nullptr) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        storeInstanceField(instance_ptr, struct_ty, type_def, attr.name.lexeme, value);
    }

    current_self_alloca_ = saved_self_alloca;
    current_self_class_ = saved_self_class;
}

void LLVMCodeGenerator::visit(parser::GetAttrExpr* expr) {
    llvm::StructType* struct_ty = nullptr;
    llvm::Value* instance_ptr = resolveInstancePtr(expr->object.get(), &struct_ty);
    if (instance_ptr == nullptr || had_error_) {
        return;
    }

    ClassInfo* info = lookupClassInfoByStruct(struct_ty);
    if (info == nullptr || info->decl == nullptr) {
        fail("codegen: tipo de instancia desconocido para lectura de atributo");
        return;
    }

    current_value_ = loadInstanceField(instance_ptr, struct_ty, info->decl, expr->name.lexeme);
    if (had_error_) {
        return;
    }

    const std::string& field_name = expr->name.lexeme;
    for (const auto& attr : info->decl->attributes) {
        if (attr.name.lexeme != field_name || !attr.declared_type.has_value()) {
            continue;
        }
        llvm::StructType* field_struct = getInstanceStructType(attr.declared_type->lexeme);
        if (field_struct != nullptr) {
            trackInstanceType(current_value_, field_struct);
        }
        break;
    }
    for (const auto& param : info->decl->params) {
        if (param.first.lexeme != field_name || !param.second.has_value()) {
            continue;
        }
        llvm::StructType* field_struct = getInstanceStructType(param.second->lexeme);
        if (field_struct != nullptr) {
            trackInstanceType(current_value_, field_struct);
        }
        break;
    }
}

void LLVMCodeGenerator::visit(parser::NewExpr* expr) {
    const std::string& type_name = expr->type_name.lexeme;
    const auto type_it = class_decls_.find(type_name);
    if (type_it == class_decls_.end()) {
        fail("Tipo no registrado: " + type_name);
        return;
    }

    parser::ClassDecl* type_def = type_it->second;
    if (type_def->params.size() != expr->args.size()) {
        fail("Numero de argumentos invalido para " + type_name);
        return;
    }

    ClassInfo* info = lookupClassInfo(type_name);
    if (info == nullptr) {
        fail("codegen: tipo de instancia no registrado");
        return;
    }

    std::vector<llvm::Value*> ctor_args;
    ctor_args.reserve(expr->args.size());
    for (std::size_t i = 0; i < expr->args.size(); ++i) {
        const std::string& param_name = type_def->params[i].first.lexeme;
        unsigned idx = 0;
        if (!lookupFieldIndex(info, param_name, &idx)) {
            return;
        }
        llvm::Type* field_ty = info->field_llvm_types[idx];

        expr->args[i]->accept(this);
        if (had_error_) {
            return;
        }
        llvm::Value* value = expectFieldValue(current_value_, field_ty, "new");
        if (value == nullptr) {
            return;
        }
        ctor_args.push_back(value);
    }

    llvm::Function* malloc_fn = getMallocFunction(*module_, *context_);
    llvm::Value* size = llvm::ConstantInt::get(
        llvm::Type::getInt64Ty(*context_), module_->getDataLayout().getTypeAllocSize(info->struct_ty));
    llvm::Value* raw_ptr = builder_->CreateCall(malloc_fn, {size});
    llvm::Value* instance_ptr = builder_->CreateBitCast(raw_ptr, opaquePtr());

    storeInstanceRuntimeType(instance_ptr, info->struct_ty, type_name);

    for (std::size_t i = 0; i < type_def->params.size(); ++i) {
        storeInstanceField(instance_ptr, info->struct_ty, type_def, type_def->params[i].first.lexeme,
                           ctor_args[i]);
    }

    initializeParentAttributes(instance_ptr, info->struct_ty, type_def);
    if (had_error_) {
        return;
    }

    llvm::AllocaInst* saved_self_alloca = current_self_alloca_;
    parser::ClassDecl* saved_self_class = current_self_class_;
    current_self_alloca_ = builder_->CreateAlloca(opaquePtr(), nullptr, "");
    builder_->CreateStore(instance_ptr, current_self_alloca_);
    current_self_class_ = type_def;

    for (const auto& attr : type_def->attributes) {
        attr.value->accept(this);
        if (had_error_) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        unsigned idx = 0;
        if (!lookupFieldIndex(info, attr.name.lexeme, &idx)) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        llvm::Type* field_ty = info->field_llvm_types[idx];
        llvm::Value* value = expectFieldValue(current_value_, field_ty, "attr init");
        if (value == nullptr) {
            current_self_alloca_ = saved_self_alloca;
            current_self_class_ = saved_self_class;
            return;
        }
        storeInstanceField(instance_ptr, info->struct_ty, type_def, attr.name.lexeme, value);
    }

    current_self_alloca_ = saved_self_alloca;
    current_self_class_ = saved_self_class;

    std::string inst_name = "inst_" + type_name;
    current_value_ = builder_->CreateBitCast(instance_ptr, opaquePtr(), inst_name);
    trackInstanceType(current_value_, info->struct_ty);
}

void LLVMCodeGenerator::visit(parser::BaseCallExpr* expr) {
    if (current_self_alloca_ == nullptr || current_self_class_ == nullptr ||
        current_method_name_.empty()) {
        fail("codegen: base() fuera de contexto de metodo");
        return;
    }

    parser::ClassDecl* parent_decl = getParentClass(current_self_class_);
    if (parent_decl == nullptr || parent_decl->name.lexeme == "Object") {
        fail("codegen: base(): metodo base no encontrado");
        return;
    }

    const MethodResolution resolution = resolveMethod(parent_decl, current_method_name_);
    if (resolution.method == nullptr || resolution.declaring_class == nullptr) {
        fail("codegen: base(): metodo base no encontrado");
        return;
    }

    if (resolution.method->params.size() != expr->args.size()) {
        fail("base(): numero de argumentos incorrecto");
        return;
    }

    llvm::Function* fn = lookupMethod(resolution.declaring_class->name.lexeme, current_method_name_,
                                      resolution.method->params.size());
    if (fn == nullptr) {
        fail("codegen: base(): metodo base no declarado");
        return;
    }

    llvm::Value* self_ptr = builder_->CreateLoad(opaquePtr(), current_self_alloca_);
    std::vector<llvm::Value*> call_args;
    call_args.push_back(self_ptr);
    for (std::size_t i = 0; i < expr->args.size(); ++i) {
        expr->args[i]->accept(this);
        if (had_error_) {
            return;
        }
        llvm::Type* param_ty = llvmTypeForHulkType(resolution.method->params[i].second);
        llvm::Value* value = expectFieldValue(current_value_, param_ty, current_method_name_);
        if (value == nullptr) {
            return;
        }
        call_args.push_back(value);
    }

    current_value_ = builder_->CreateCall(fn, call_args);
}

void LLVMCodeGenerator::visit(parser::IsExpr* expr) {
    expr->object->accept(this);
    if (had_error_) {
        return;
    }

    const std::string& target = expr->type_name.lexeme;
    if (class_decls_.find(target) == class_decls_.end() &&
        target != "Number" && target != "String" && target != "Boolean" && target != "Null") {
        fail("Tipo no registrado: " + target);
        return;
    }

    if (isInstanceValue(current_value_)) {
        llvm::StructType* struct_ty = resolveInstanceStructType(current_value_);
        if (struct_ty == nullptr) {
            fail("codegen: tipo de instancia desconocido en 'is'");
            return;
        }
        llvm::Value* runtime_type = loadInstanceRuntimeType(current_value_, struct_ty);
        current_value_ = emitRuntimeTypeConforms(runtime_type, target);
        return;
    }

    current_value_ = emitScalarIsCheck(current_value_, target);
}

void LLVMCodeGenerator::visit(parser::AsExpr* expr) {
    expr->object->accept(this);
    if (had_error_) {
        return;
    }

    const std::string& target = expr->type_name.lexeme;

    if (!isInstanceValue(current_value_)) {
        if (target == "Null" && llvm::isa<llvm::ConstantPointerNull>(current_value_)) {
            return;
        }
        if (target == "Number" && current_value_->getType()->isDoubleTy()) {
            return;
        }
        if (target == "Boolean" && current_value_->getType()->isIntegerTy(1)) {
            return;
        }
        if (target == "String" && isBoxedValue(current_value_)) {
            return;
        }
        fail("No se puede convertir valor a " + target);
        return;
    }

    if (getInstanceStructType(target) == nullptr) {
        fail("Tipo no registrado: " + target);
        return;
    }

    llvm::StructType* struct_ty = resolveInstanceStructType(current_value_);
    if (struct_ty == nullptr) {
        fail("codegen: tipo de instancia desconocido en 'as'");
        return;
    }

    llvm::Value* runtime_type = loadInstanceRuntimeType(current_value_, struct_ty);
    llvm::Value* conforms = emitRuntimeTypeConforms(runtime_type, target);

    llvm::Function* parent_fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* ok_bb = llvm::BasicBlock::Create(*context_, "as.ok", parent_fn);
    llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*context_, "as.fail", parent_fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "as.end", parent_fn);

    builder_->CreateCondBr(conforms, ok_bb, fail_bb);
    builder_->SetInsertPoint(fail_bb);
    emitCastFailure(expr->as_keyword.line, expr->as_keyword.col);
    builder_->SetInsertPoint(ok_bb);
    llvm::StructType* target_struct = getInstanceStructType(target);
    llvm::Value* casted = builder_->CreateBitCast(current_value_, opaquePtr(), "inst_" + target);
    trackInstanceType(casted, target_struct);
    builder_->CreateBr(merge_bb);
    builder_->SetInsertPoint(merge_bb);
    current_value_ = casted;
}

void LLVMCodeGenerator::visit(parser::MethodCallExpr* expr) {
    emitMethodCall(expr->object.get(), expr->method_name.lexeme, expr->args);
}

void LLVMCodeGenerator::visit(parser::SetAttrExpr* expr) {
    llvm::StructType* struct_ty = nullptr;
    llvm::Value* instance_ptr = resolveInstancePtr(expr->object.get(), &struct_ty);
    if (instance_ptr == nullptr || had_error_) {
        return;
    }

    ClassInfo* info = lookupClassInfoByStruct(struct_ty);
    if (info == nullptr || info->decl == nullptr) {
        fail("codegen: tipo de instancia desconocido para asignacion");
        return;
    }

    expr->value->accept(this);
    if (had_error_) {
        return;
    }
    unsigned idx = 0;
    if (!lookupFieldIndex(info, expr->attr_name.lexeme, &idx)) {
        return;
    }
    llvm::Type* field_ty = info->field_llvm_types[idx];
    llvm::Value* value = expectFieldValue(current_value_, field_ty, "set attr");
    if (value == nullptr) {
        return;
    }
    storeInstanceField(instance_ptr, struct_ty, info->decl, expr->attr_name.lexeme, value);
    current_value_ = value;
}

void LLVMCodeGenerator::visit(parser::UnaryExpr* expr) {
    expr->right->accept(this);
    if (had_error_) {
        return;
    }

    const std::string& op = expr->op.lexeme;
    if (op == "-") {
        llvm::Value* val = current_value_;
        if (!val->getType()->isDoubleTy()) {
            val = unboxDoubleValue(val, op);
        }
        if (val == nullptr) {
            return;
        }
        current_value_ = builder_->CreateFNeg(val, "neg");
        return;
    }

    if (op == "!" || op == "not") {
        llvm::Value* val = current_value_;
        if (!val->getType()->isIntegerTy(1)) {
            val = unboxBoolValue(val, op);
        }
        if (val == nullptr) {
            return;
        }
        current_value_ = builder_->CreateNot(val, "not");
        return;
    }

    fail("Operador unario no soportado: " + op);
}

void LLVMCodeGenerator::visit(parser::GroupedExpr* expr) {
    expr->expression->accept(this);
}

void LLVMCodeGenerator::visit(parser::UnlessExpr* expr) {
    llvm::BasicBlock* cond_bb = builder_->GetInsertBlock();

    expr->condition->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* cond = expectBoolValue(current_value_, "unless");
    if (cond == nullptr) {
        return;
    }

    llvm::Function* fn = cond_bb->getParent();
    llvm::BasicBlock* then_bb = llvm::BasicBlock::Create(*context_, "unless.then", fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "unless.end", fn);
    llvm::BasicBlock* else_bb = nullptr;

    if (expr->else_branch) {
        else_bb = llvm::BasicBlock::Create(*context_, "unless.else", fn);
        builder_->CreateCondBr(cond, else_bb, then_bb);
    } else {
        builder_->CreateCondBr(cond, merge_bb, then_bb);
    }

    builder_->SetInsertPoint(then_bb);
    expr->then_branch->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* then_val = current_value_;
    if (then_val == nullptr) {
        fail("codegen: rama principal de unless sin valor");
        return;
    }
    llvm::Type* result_ty = then_val->getType();
    builder_->CreateBr(merge_bb);
    llvm::BasicBlock* then_end = builder_->GetInsertBlock();

    llvm::BasicBlock* else_end = nullptr;
    llvm::Value* else_val = nullptr;
    if (expr->else_branch) {
        builder_->SetInsertPoint(else_bb);
        expr->else_branch->accept(this);
        if (had_error_) {
            return;
        }
        else_val = current_value_;
        if (else_val == nullptr) {
            fail("codegen: rama 'else' de unless sin valor");
            return;
        }
        builder_->CreateBr(merge_bb);
        else_end = builder_->GetInsertBlock();
    }

    builder_->SetInsertPoint(merge_bb);
    if (expr->else_branch) {
        llvm::PHINode* phi = builder_->CreatePHI(result_ty, 2, "unless.result");
        phi->addIncoming(then_val, then_end);
        phi->addIncoming(else_val, else_end);
        current_value_ = phi;
    } else {
        llvm::PHINode* phi = builder_->CreatePHI(result_ty, 2, "unless.result");
        phi->addIncoming(then_val, then_end);
        phi->addIncoming(defaultValueForType(result_ty), cond_bb);
        current_value_ = phi;
    }
}

void LLVMCodeGenerator::visit(parser::RepeatExpr* expr) {
    expr->count->accept(this);
    if (had_error_) {
        return;
    }

    llvm::Type* i32_ty = llvm::Type::getInt32Ty(*context_);
    llvm::Value* count_val = unboxDoubleValue(current_value_, "repeat");
    if (count_val == nullptr) {
        return;
    }
    llvm::Value* times = builder_->CreateFPToSI(count_val, i32_ty, "repeat.times");

    llvm::AllocaInst* result_slot = builder_->CreateAlloca(opaquePtr(), nullptr, "repeat.last");
    llvm::AllocaInst* index_slot = builder_->CreateAlloca(i32_ty, nullptr, "repeat.i");

    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* zero_bb = llvm::BasicBlock::Create(*context_, "repeat.zero", fn);
    llvm::BasicBlock* init_bb = llvm::BasicBlock::Create(*context_, "repeat.init", fn);
    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context_, "repeat.cond", fn);
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "repeat.body", fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*context_, "repeat.end", fn);

    llvm::Value* is_nonpos =
        builder_->CreateICmpSLE(times, llvm::ConstantInt::get(i32_ty, 0), "repeat.nonpos");
    builder_->CreateCondBr(is_nonpos, zero_bb, init_bb);

    builder_->SetInsertPoint(zero_bb);
    builder_->CreateStore(llvm::ConstantPointerNull::get(opaquePtr()), result_slot);
    builder_->CreateBr(end_bb);

    builder_->SetInsertPoint(init_bb);
    builder_->CreateStore(llvm::ConstantInt::get(i32_ty, 0), index_slot);
    builder_->CreateStore(llvm::ConstantPointerNull::get(opaquePtr()), result_slot);
    builder_->CreateBr(cond_bb);

    builder_->SetInsertPoint(cond_bb);
    llvm::Value* index_val = builder_->CreateLoad(i32_ty, index_slot);
    llvm::Value* continue_loop = builder_->CreateICmpSLT(index_val, times, "repeat.cont");
    builder_->CreateCondBr(continue_loop, body_bb, end_bb);

    builder_->SetInsertPoint(body_bb);
    expr->body->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* stored = boxValueForStorage(current_value_);
    if (stored == nullptr || had_error_) {
        return;
    }
    builder_->CreateStore(stored, result_slot);
    llvm::Value* next_index =
        builder_->CreateAdd(index_val, llvm::ConstantInt::get(i32_ty, 1), "repeat.next_i");
    builder_->CreateStore(next_index, index_slot);
    builder_->CreateBr(cond_bb);

    builder_->SetInsertPoint(end_bb);
    current_value_ = builder_->CreateLoad(opaquePtr(), result_slot);
}

void LLVMCodeGenerator::visit(parser::LoopWhileExpr* expr) {
    llvm::Function* fn = builder_->GetInsertBlock()->getParent();
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "loopwhile.body", fn);
    llvm::BasicBlock* cond_bb = llvm::BasicBlock::Create(*context_, "loopwhile.cond", fn);
    llvm::BasicBlock* end_bb = llvm::BasicBlock::Create(*context_, "loopwhile.end", fn);

    llvm::AllocaInst* result_slot = builder_->CreateAlloca(opaquePtr(), nullptr, "loopwhile.last");

    builder_->CreateBr(body_bb);

    builder_->SetInsertPoint(body_bb);
    expr->body->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* stored = boxValueForStorage(current_value_);
    if (stored == nullptr || had_error_) {
        return;
    }
    builder_->CreateStore(stored, result_slot);
    builder_->CreateBr(cond_bb);

    builder_->SetInsertPoint(cond_bb);
    expr->condition->accept(this);
    if (had_error_) {
        return;
    }
    llvm::Value* cond = expectBoolValue(current_value_, "loop-while");
    if (cond == nullptr) {
        return;
    }
    builder_->CreateCondBr(cond, body_bb, end_bb);

    builder_->SetInsertPoint(end_bb);
    current_value_ = builder_->CreateLoad(opaquePtr(), result_slot);
}

void LLVMCodeGenerator::visit(parser::WithExpr* expr) {
    llvm::BasicBlock* entry_bb = builder_->GetInsertBlock();

    expr->value->accept(this);
    if (had_error_) {
        return;
    }

    llvm::Value* bound = current_value_;
    if (bound == nullptr) {
        fail("codegen: with sin valor fuente");
        return;
    }

    llvm::Function* fn = entry_bb->getParent();
    llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "with.body", fn);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "with.end", fn);
    llvm::BasicBlock* else_bb = nullptr;

    llvm::Value* is_null_val = llvm::ConstantInt::getFalse(*context_);
    if (bound->getType()->isPointerTy()) {
        is_null_val = builder_->CreateICmpEQ(bound, llvm::ConstantPointerNull::get(opaquePtr()));
    }

    if (expr->else_branch) {
        else_bb = llvm::BasicBlock::Create(*context_, "with.else", fn);
        builder_->CreateCondBr(is_null_val, else_bb, body_bb);
    } else {
        builder_->CreateCondBr(is_null_val, merge_bb, body_bb);
    }

    builder_->SetInsertPoint(body_bb);
    llvm::Type* semantic_ty = classifySemanticType(bound);
    llvm::Type* storage_ty = bound->getType();
    if (bound->getType()->isPointerTy()) {
        storage_ty = opaquePtr();
    }

    enterScope();
    llvm::AllocaInst* alias_alloca = builder_->CreateAlloca(storage_ty, nullptr, "");
    builder_->CreateStore(bound, alias_alloca);
    current_scope_->variables[expr->alias.lexeme] = alias_alloca;
    current_scope_->variable_types[expr->alias.lexeme] = semantic_ty;

    expr->body->accept(this);
    if (had_error_) {
        exitScope();
        return;
    }
    llvm::Value* body_val = current_value_;
    if (body_val == nullptr) {
        fail("codegen: cuerpo de with sin valor");
        exitScope();
        return;
    }
    llvm::Type* result_ty = body_val->getType();
    if (expr->else_branch) {
        body_val = boxValueForStorage(body_val);
        if (body_val == nullptr || had_error_) {
            exitScope();
            return;
        }
    }
    exitScope();
    builder_->CreateBr(merge_bb);
    llvm::BasicBlock* body_end = builder_->GetInsertBlock();

    llvm::BasicBlock* else_end = nullptr;
    llvm::Value* else_val = nullptr;
    if (expr->else_branch) {
        builder_->SetInsertPoint(else_bb);
        expr->else_branch->accept(this);
        if (had_error_) {
            return;
        }
        else_val = current_value_;
        if (else_val == nullptr) {
            fail("codegen: rama else de with sin valor");
            return;
        }
        else_val = boxValueForStorage(else_val);
        if (else_val == nullptr || had_error_) {
            return;
        }
        builder_->CreateBr(merge_bb);
        else_end = builder_->GetInsertBlock();
    }

    builder_->SetInsertPoint(merge_bb);
    if (expr->else_branch) {
        llvm::PHINode* phi = builder_->CreatePHI(opaquePtr(), 2, "with.result");
        phi->addIncoming(body_val, body_end);
        phi->addIncoming(else_val, else_end);
        current_value_ = phi;
    } else {
        llvm::PHINode* phi = builder_->CreatePHI(result_ty, 2, "with.result");
        phi->addIncoming(body_val, body_end);
        phi->addIncoming(defaultValueForType(result_ty), entry_bb);
        current_value_ = phi;
    }
}

void LLVMCodeGenerator::visit(parser::CaseExpr* expr) {
    llvm::BasicBlock* entry_bb = builder_->GetInsertBlock();

    expr->value->accept(this);
    if (had_error_) {
        return;
    }

    llvm::Value* scrutinee = current_value_;
    if (scrutinee == nullptr) {
        fail("codegen: case sin valor fuente");
        return;
    }

    if (expr->branches.empty()) {
        fail("codegen: case sin ramas");
        return;
    }

    llvm::Function* fn = entry_bb->getParent();
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(*context_, "case.end", fn);
    llvm::BasicBlock* fail_bb = llvm::BasicBlock::Create(*context_, "case.fail", fn);

    std::vector<std::size_t> branch_order(expr->branches.size());
    std::iota(branch_order.begin(), branch_order.end(), 0);
    std::sort(branch_order.begin(), branch_order.end(),
              [&](std::size_t i, std::size_t j) {
                  const std::string& a = expr->branches[i].type_name.lexeme;
                  const std::string& b = expr->branches[j].type_name.lexeme;
                  const auto a_it = class_decls_.find(a);
                  const auto b_it = class_decls_.find(b);
                  if (a_it != class_decls_.end() && b_it != class_decls_.end()) {
                      if (instanceConformsTo(a_it->second, b) && a != b) {
                          return true;
                      }
                      if (instanceConformsTo(b_it->second, a) && a != b) {
                          return false;
                      }
                  }
                  return false;
              });

    llvm::BasicBlock* next_bb = fail_bb;
    std::vector<llvm::BasicBlock*> branch_end_blocks;
    std::vector<llvm::Value*> branch_values;
    llvm::Type* result_ty = nullptr;
    llvm::BasicBlock* entry_check_bb = nullptr;

    for (const std::size_t idx : branch_order) {
        const auto& branch = expr->branches[idx];
        const std::string& target = branch.type_name.lexeme;

        llvm::BasicBlock* check_bb = llvm::BasicBlock::Create(*context_, "case.check", fn);
        llvm::BasicBlock* body_bb = llvm::BasicBlock::Create(*context_, "case.body", fn);
        if (entry_check_bb == nullptr) {
            entry_check_bb = check_bb;
        }

        builder_->SetInsertPoint(check_bb);

        llvm::Value* matches = nullptr;
        if (isInstanceValue(scrutinee)) {
            llvm::StructType* struct_ty = resolveInstanceStructType(scrutinee);
            if (struct_ty == nullptr) {
                fail("codegen: tipo de instancia desconocido en case");
                return;
            }
            llvm::Value* runtime_type = loadInstanceRuntimeType(scrutinee, struct_ty);
            matches = emitRuntimeTypeConforms(runtime_type, target);
        } else {
            matches = emitScalarIsCheck(scrutinee, target);
        }

        builder_->CreateCondBr(matches, body_bb, next_bb);

        builder_->SetInsertPoint(body_bb);
        enterScope();
        llvm::Type* semantic_ty = classifySemanticType(scrutinee);
        llvm::Type* storage_ty = scrutinee->getType();
        if (scrutinee->getType()->isPointerTy()) {
            storage_ty = opaquePtr();
        }
        llvm::AllocaInst* alias_alloca = builder_->CreateAlloca(storage_ty, nullptr, "");
        builder_->CreateStore(scrutinee, alias_alloca);
        current_scope_->variables[branch.name.lexeme] = alias_alloca;
        current_scope_->variable_types[branch.name.lexeme] = semantic_ty;

        branch.body->accept(this);
        if (had_error_) {
            exitScope();
            return;
        }
        llvm::Value* body_val = current_value_;
        if (body_val == nullptr) {
            fail("codegen: rama de case sin valor");
            exitScope();
            return;
        }
        if (result_ty == nullptr) {
            result_ty = body_val->getType();
        }
        exitScope();
        builder_->CreateBr(merge_bb);
        branch_end_blocks.push_back(builder_->GetInsertBlock());
        branch_values.push_back(body_val);
        next_bb = check_bb;
    }

    builder_->SetInsertPoint(fail_bb);
    emitCaseFailure(expr->branches.front().type_name.line, expr->branches.front().type_name.col);

    builder_->SetInsertPoint(entry_bb);
    builder_->CreateBr(entry_check_bb);

    builder_->SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder_->CreatePHI(result_ty, branch_values.size(), "case.result");
    for (std::size_t i = 0; i < branch_values.size(); ++i) {
        phi->addIncoming(branch_values[i], branch_end_blocks[i]);
    }
    current_value_ = phi;
}

}  // namespace hulk::codegen
