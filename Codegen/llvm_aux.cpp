#include "llvm_aux.hpp"

#include <llvm/IR/Function.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>

namespace hulk::codegen {

void registerRuntimeDeclarations(llvm::Module& module, llvm::LLVMContext& context) {
    llvm::FunctionType* time_ty =
        llvm::FunctionType::get(llvm::Type::getInt64Ty(context),
                                {llvm::PointerType::getUnqual(llvm::Type::getInt64Ty(context))},
                                false);
    module.getOrInsertFunction("time", time_ty);

    llvm::FunctionType* srand_ty =
        llvm::FunctionType::get(llvm::Type::getVoidTy(context),
                                {llvm::Type::getInt32Ty(context)}, false);
    module.getOrInsertFunction("srand", srand_ty);

    llvm::FunctionType* malloc_ty = llvm::FunctionType::get(
        llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(context)),
        {llvm::Type::getInt64Ty(context)},
        false);
    module.getOrInsertFunction("malloc", malloc_ty);

    llvm::FunctionType* pow_ty = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context),
        {llvm::Type::getDoubleTy(context), llvm::Type::getDoubleTy(context)},
        false);
    module.getOrInsertFunction("pow", pow_ty);

    llvm::FunctionType* fmod_ty = llvm::FunctionType::get(
        llvm::Type::getDoubleTy(context),
        {llvm::Type::getDoubleTy(context), llvm::Type::getDoubleTy(context)},
        false);
    module.getOrInsertFunction("fmod", fmod_ty);
}

void registerBuiltinDeclarations(llvm::Module& module, llvm::LLVMContext& context) {
    llvm::Type* double_ty = llvm::Type::getDoubleTy(context);
    llvm::FunctionType* unary_double_ty =
        llvm::FunctionType::get(double_ty, {double_ty}, false);

    for (const char* name : {"sin", "cos", "sqrt", "log", "exp"}) {
        module.getOrInsertFunction(name, unary_double_ty);
    }

    llvm::FunctionType* rand_ty = llvm::FunctionType::get(llvm::Type::getInt32Ty(context), false);
    module.getOrInsertFunction("rand", rand_ty);
}

}  // namespace hulk::codegen
