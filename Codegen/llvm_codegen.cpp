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
    registerRuntimeDeclarations(*module_, *context_);
    registerMathematicalConstants();
    registerPrintDeclarations();
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

    for (LLVMScope* scope = current_scope_; scope != nullptr; scope = scope->parent) {
        const auto found = scope->variables.find(name);
        if (found != scope->variables.end()) {
            llvm::AllocaInst* alloca = found->second;
            llvm::Type* var_type = scope->variable_types.at(name);
            current_value_ = builder_->CreateLoad(var_type, alloca);
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
    llvm::Type* var_type = init_value->getType();
    const std::string var_name = expr->name.lexeme;

    enterScope();

    llvm::AllocaInst* alloca = builder_->CreateAlloca(var_type, nullptr, var_name);
    builder_->CreateStore(init_value, alloca);
    current_scope_->variables[var_name] = alloca;
    current_scope_->variable_types[var_name] = var_type;

    expr->body->accept(this);

    exitScope();
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
        fail("codegen: solo llamadas a print builtin soportadas en I4");
        return;
    }

    const auto* callee = static_cast<parser::IdentifierExpr*>(expr->callee.get());
    const std::string& name = callee->token.lexeme;
    if (name != "print") {
        fail("codegen: funcion '" + name + "' no implementada (iteracion posterior a I4)");
        return;
    }

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

HULK_VISIT_STUB(FunctionDecl)
HULK_VISIT_STUB(ClassDecl)
HULK_VISIT_STUB(MethodDecl)
HULK_VISIT_STUB(AttributeDecl)
HULK_VISIT_STUB(SelfExpr)
HULK_VISIT_STUB(GroupedExpr)
HULK_VISIT_STUB(UnaryExpr)
HULK_VISIT_STUB(GetAttrExpr)
HULK_VISIT_STUB(BlockExpr)
HULK_VISIT_STUB(IfExpr)
HULK_VISIT_STUB(WhileExpr)
HULK_VISIT_STUB(ForExpr)
HULK_VISIT_STUB(WithExpr)
HULK_VISIT_STUB(CaseExpr)
HULK_VISIT_STUB(AsExpr)
HULK_VISIT_STUB(AssignExpr)
HULK_VISIT_STUB(NewExpr)
HULK_VISIT_STUB(BaseCallExpr)
HULK_VISIT_STUB(SetAttrExpr)
HULK_VISIT_STUB(MethodCallExpr)
HULK_VISIT_STUB(UnlessExpr)
HULK_VISIT_STUB(RepeatExpr)
HULK_VISIT_STUB(LoopWhileExpr)

}  // namespace hulk::codegen
