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

#define HULK_VISIT_STUB(Node) \
    void LLVMCodeGenerator::visit(parser::Node* /*node*/) { \
        fail("codegen no implementado para " #Node " (iteracion posterior a I2)"); \
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
        ".str." + std::to_string(var_counter_++));
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
        const std::string load_name = "const_" + name;
        current_value_ = builder_->CreateLoad(global->getValueType(), global, load_name);
        return;
    }

    for (LLVMScope* scope = current_scope_; scope != nullptr; scope = scope->parent) {
        const auto found = scope->variables.find(name);
        if (found != scope->variables.end()) {
            llvm::AllocaInst* alloca = found->second;
            llvm::Type* var_type = scope->variable_types.at(name);
            const std::string load_name = "var_" + name;
            current_value_ = builder_->CreateLoad(var_type, alloca, load_name);
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

HULK_VISIT_STUB(FunctionDecl)
HULK_VISIT_STUB(ClassDecl)
HULK_VISIT_STUB(MethodDecl)
HULK_VISIT_STUB(AttributeDecl)
HULK_VISIT_STUB(SelfExpr)
HULK_VISIT_STUB(GroupedExpr)
HULK_VISIT_STUB(UnaryExpr)
HULK_VISIT_STUB(BinaryExpr)
HULK_VISIT_STUB(CallExpr)
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
