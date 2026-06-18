#pragma once

namespace llvm {
class LLVMContext;
class Module;
}  // namespace llvm

namespace hulk::codegen {

void registerRuntimeDeclarations(llvm::Module& module, llvm::LLVMContext& context);
void registerBuiltinDeclarations(llvm::Module& module, llvm::LLVMContext& context);

}  // namespace hulk::codegen
