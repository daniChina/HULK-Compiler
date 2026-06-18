#include "llvm_codegen.hpp"

#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ADT/SmallString.h>
#include <llvm/Support/raw_ostream.h>

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
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)),
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
    builder_ = std::make_unique<llvm::IRBuilder<>>(*context_);
    scopes_.clear();
    scopes_.push_back(std::make_unique<LLVMScope>());
    current_scope_ = scopes_.back().get();
    user_functions_.clear();
    user_function_names_.clear();
    registerRuntimeDeclarations(*module_, *context_);
    registerMathematicalConstants();
    registerPrintDeclarations();
    registerBuiltinDeclarations(*module_, *context_);
    registerRangeDeclarations();
}

void LLVMCodeGenerator::registerRangeDeclarations() {
    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    llvm::PointerType* range_ptr = llvm::PointerType::getUnqual(getRangeType());
    llvm::FunctionType* range_create_ty =
        llvm::FunctionType::get(range_ptr, {double_ty, double_ty}, false);
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

    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    std::vector<llvm::Type*> param_types(arity, double_ty);
    llvm::FunctionType* fn_type = llvm::FunctionType::get(double_ty, param_types, false);
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
    LLVMScope* saved_scope = current_scope_;
    const std::size_t saved_scope_depth = scopes_.size();

    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", fn);
    builder_->SetInsertPoint(entry);

    scopes_.clear();
    scopes_.push_back(std::make_unique<LLVMScope>());
    current_scope_ = scopes_.back().get();

    llvm::Type* double_ty = llvm::Type::getDoubleTy(*context_);
    llvm::Function::arg_iterator arg_it = fn->arg_begin();
    for (const auto& param : decl->params) {
        const std::string& param_name = param.first.lexeme;
        llvm::AllocaInst* alloca = builder_->CreateAlloca(double_ty, nullptr, "");
        builder_->CreateStore(&*arg_it, alloca);
        current_scope_->variables[param_name] = alloca;
        current_scope_->variable_types[param_name] = double_ty;
        ++arg_it;
    }

    decl->body->accept(this);
    if (had_error_) {
        scopes_.resize(saved_scope_depth);
        current_scope_ = saved_scope;
        if (saved_insert_block != nullptr) {
            builder_->SetInsertPoint(saved_insert_block);
        }
        return;
    }

    llvm::Value* result = current_value_;
    if (result == nullptr) {
        result = llvm::ConstantFP::get(double_ty, 0.0);
    } else if (!result->getType()->isDoubleTy()) {
        fail("La funcion '" + name + "' debe devolver un valor numerico en I7");
        scopes_.resize(saved_scope_depth);
        current_scope_ = saved_scope;
        if (saved_insert_block != nullptr) {
            builder_->SetInsertPoint(saved_insert_block);
        }
        return;
    }

    builder_->CreateRet(result);

    scopes_.resize(saved_scope_depth);
    current_scope_ = saved_scope;
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

llvm::Value* LLVMCodeGenerator::invokeUserFunction(llvm::Function* fn,
                                                   const std::vector<llvm::Value*>& args) {
    return builder_->CreateCall(fn, args);
}

void LLVMCodeGenerator::registerPrintDeclarations() {
    llvm::StructType* boxed_ty = getBoxedValueType();
    llvm::PointerType* boxed_ptr = llvm::PointerType::getUnqual(boxed_ty);

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
        llvm::FunctionType::get(llvm::Type::getVoidTy(*context_), {boxed_ptr}, false);
    module_->getOrInsertFunction("hulk_print_boxed", print_boxed_ty);

    llvm::FunctionType* concat_ty =
        llvm::FunctionType::get(boxed_ptr, {boxed_ptr, boxed_ptr}, false);
    module_->getOrInsertFunction("hulk_string_concat", concat_ty);
    module_->getOrInsertFunction("hulk_string_concat_ws", concat_ty);
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
                *out_type = scope->variable_types.at(name);
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
    llvm::StructType* boxed_ty = getBoxedValueType();
    llvm::Function* malloc_fn = getMallocFunction(*module_, *context_);

    llvm::Value* size = llvm::ConstantExpr::getSizeOf(boxed_ty);
    llvm::Value* raw_ptr = builder_->CreateCall(malloc_fn, {size});
    llvm::Value* boxed =
        builder_->CreateBitCast(raw_ptr, llvm::PointerType::getUnqual(boxed_ty), "boxed_double");

    builder_->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 1),
                          builder_->CreateStructGEP(boxed_ty, boxed, 0));

    llvm::Value* data_ptr = builder_->CreateStructGEP(boxed_ty, boxed, 1);
    data_ptr = builder_->CreatePointerCast(data_ptr, llvm::PointerType::getUnqual(llvm::Type::getDoubleTy(*context_)));
    builder_->CreateStore(double_val, data_ptr);
    return boxed;
}

llvm::Value* LLVMCodeGenerator::createBoxedFromBool(llvm::Value* bool_val) {
    llvm::StructType* boxed_ty = getBoxedValueType();
    llvm::Function* malloc_fn = getMallocFunction(*module_, *context_);

    llvm::Value* size = llvm::ConstantExpr::getSizeOf(boxed_ty);
    llvm::Value* raw_ptr = builder_->CreateCall(malloc_fn, {size});
    llvm::Value* boxed =
        builder_->CreateBitCast(raw_ptr, llvm::PointerType::getUnqual(boxed_ty), "boxed_bool");

    builder_->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 0),
                          builder_->CreateStructGEP(boxed_ty, boxed, 0));

    llvm::Value* data_ptr = builder_->CreateStructGEP(boxed_ty, boxed, 1);
    data_ptr = builder_->CreatePointerCast(data_ptr, llvm::PointerType::getUnqual(llvm::Type::getInt1Ty(*context_)));
    builder_->CreateStore(bool_val, data_ptr);
    return boxed;
}

llvm::Value* LLVMCodeGenerator::createBoxedFromString(llvm::Value* str_val) {
    llvm::StructType* boxed_ty = getBoxedValueType();
    llvm::Function* malloc_fn = getMallocFunction(*module_, *context_);

    llvm::Value* size = llvm::ConstantExpr::getSizeOf(boxed_ty);
    llvm::Value* raw_ptr = builder_->CreateCall(malloc_fn, {size});
    llvm::Value* boxed =
        builder_->CreateBitCast(raw_ptr, llvm::PointerType::getUnqual(boxed_ty), "boxed_str");

    builder_->CreateStore(llvm::ConstantInt::get(llvm::Type::getInt32Ty(*context_), 2),
                          builder_->CreateStructGEP(boxed_ty, boxed, 0));

    llvm::Value* data_ptr = builder_->CreateStructGEP(boxed_ty, boxed, 1);
    llvm::Value* slot = builder_->CreateBitCast(data_ptr, llvm::PointerType::getUnqual(str_val->getType()));
    builder_->CreateStore(str_val, slot);
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
        if (child->stmt_kind == parser::StmtKind::FUNCTION_DECL) {
            defineUserFunction(static_cast<parser::FunctionDecl*>(child.get()));
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
        return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(type->getPointerElementType()));
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
    current_value_ = llvm::ConstantPointerNull::get(
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(*context_)));
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
        current_value_ = builder_->CreateLoad(var_type, alloca);
        return;
    }

    fail("codegen: variable '" + name + "' no definida");
}

void LLVMCodeGenerator::visit(parser::LetExpr* expr) {
    expr->initializer->accept(this);
    if (had_error_) {
        return;
    }

    llvm::Value* init_value = current_value_;
    llvm::Type* var_type = init_value->getType();
    const std::string var_name = expr->name.lexeme;

    enterScope();

    llvm::AllocaInst* alloca = builder_->CreateAlloca(var_type, nullptr, "");
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

bool LLVMCodeGenerator::isRangePointer(llvm::Type* type) {
    if (type == nullptr || !type->isPointerTy()) {
        return false;
    }
    return type->getPointerElementType() == getRangeType();
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

bool LLVMCodeGenerator::isBoxedValuePointer(llvm::Type* type) {
    if (type == nullptr || !type->isPointerTy()) {
        return false;
    }
    llvm::StructType* boxed_ty = getBoxedValueType();
    return type->getPointerElementType() == boxed_ty;
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

    if (isBoxedValuePointer(value->getType())) {
        llvm::Function* fn = module_->getFunction("hulk_print_boxed");
        builder_->CreateCall(fn, {value});
        return;
    }

    llvm::Function* null_fn = module_->getFunction("hulk_print_null");
    builder_->CreateCall(null_fn, {});
}

void LLVMCodeGenerator::visit(parser::CallExpr* expr) {
    if (expr->callee->kind != parser::ExprKind::IDENTIFIER) {
        fail("codegen: solo llamadas a identificadores soportadas en I7");
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

    llvm::Function* fn = lookupUserFunction(name, expr->args.size());
    if (fn == nullptr) {
        fail("La funcion '" + name + "' no esta definida con aridad " + std::to_string(expr->args.size()));
        return;
    }

    std::vector<llvm::Value*> arg_values;
    arg_values.reserve(expr->args.size());
    for (const auto& arg : expr->args) {
        arg->accept(this);
        if (had_error_) {
            return;
        }
        llvm::Value* value = expectDoubleValue(current_value_, name);
        if (value == nullptr) {
            return;
        }
        arg_values.push_back(value);
    }

    current_value_ = invokeUserFunction(fn, arg_values);
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
        if (left->getType()->isDoubleTy() && right->getType()->isDoubleTy()) {
            llvm::CmpInst::Predicate pred = llvm::CmpInst::FCMP_OEQ;
            if (op == "<") {
                pred = llvm::CmpInst::FCMP_OLT;
            } else if (op == "<=") {
                pred = llvm::CmpInst::FCMP_OLE;
            } else if (op == ">") {
                pred = llvm::CmpInst::FCMP_OGT;
            } else if (op == ">=") {
                pred = llvm::CmpInst::FCMP_OGE;
            } else if (op == "!=") {
                pred = llvm::CmpInst::FCMP_ONE;
            }
            current_value_ = builder_->CreateFCmp(pred, left, right, "cmp");
            return;
        }

        if (left->getType()->isIntegerTy(1) && right->getType()->isIntegerTy(1)) {
            if (op == "==") {
                current_value_ = builder_->CreateICmpEQ(left, right, "cmp");
            } else if (op == "!=") {
                current_value_ = builder_->CreateICmpNE(left, right, "cmp");
            } else {
                fail("Operador binario no soportado: " + op);
            }
            return;
        }

        fail("Operador binario no soportado: " + op);
        return;
    }

    if (op == "@" || op == "@@") {
        if (!isBoxedValuePointer(left->getType()) || !isBoxedValuePointer(right->getType())) {
            fail("Operador '" + op + "' requiere operandos string");
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
        fail("codegen: asignacion ':=' solo a variable local en I6 (attrs/metodos en I9+)");
        return;
    }

    const auto* id = static_cast<parser::IdentifierExpr*>(expr->lhs.get());

    const std::string& name = id->token.lexeme;
    llvm::AllocaInst* alloca = nullptr;
    llvm::Type* var_type = nullptr;
    if (!lookupLocalVariable(name, &alloca, &var_type)) {
        fail("codegen: variable '" + name + "' no definida para asignacion");
        return;
    }

    if (rhs_value->getType() != var_type) {
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

    llvm::Value* range_ptr = current_value_;
    if (!isRangePointer(range_ptr->getType())) {
        fail("for requiere un iterable (p. ej. range)");
        return;
    }

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
}

void LLVMCodeGenerator::visit(parser::FunctionDecl* /*stmt*/) {}

HULK_VISIT_STUB(ClassDecl)
HULK_VISIT_STUB(MethodDecl)
HULK_VISIT_STUB(AttributeDecl)
HULK_VISIT_STUB(SelfExpr)
HULK_VISIT_STUB(GroupedExpr)
HULK_VISIT_STUB(UnaryExpr)
HULK_VISIT_STUB(GetAttrExpr)
HULK_VISIT_STUB(WithExpr)
HULK_VISIT_STUB(CaseExpr)
HULK_VISIT_STUB(AsExpr)
HULK_VISIT_STUB(NewExpr)
HULK_VISIT_STUB(BaseCallExpr)
HULK_VISIT_STUB(SetAttrExpr)
HULK_VISIT_STUB(MethodCallExpr)
HULK_VISIT_STUB(UnlessExpr)
HULK_VISIT_STUB(RepeatExpr)
HULK_VISIT_STUB(LoopWhileExpr)

}  // namespace hulk::codegen
