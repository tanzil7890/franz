#include "llvm_codegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// LLVM IR Generation - Main implementation in llvm_ir_gen.c
extern LLVMCodeGen *LLVMCodeGen_new_impl(const char *moduleName);
extern void LLVMCodeGen_free_impl(LLVMCodeGen *gen);
extern int LLVMCodeGen_compile_impl(LLVMCodeGen *gen, AstNode *ast, Scope *globalScope);

// LLVM Code Generator - Public API

LLVMCodeGen *LLVMCodeGen_new(const char *moduleName) {
  return LLVMCodeGen_new_impl(moduleName);
}

void LLVMCodeGen_free(LLVMCodeGen *gen) {
  LLVMCodeGen_free_impl(gen);
}

void LLVMCodeGen_dumpIR(LLVMCodeGen *gen) {
  if (!gen || !gen->module) return;
  LLVMDumpModule(gen->module);
}

int LLVMCodeGen_writeToFile(LLVMCodeGen *gen, const char *filename) {
  if (!gen || !gen->module) return -1;

  if (LLVMWriteBitcodeToFile(gen->module, filename)) {
    fprintf(stderr, "Failed to write bitcode to file: %s\n", filename);
    return -1;
  }

  return 0;
}

// Compilation entry point
int LLVMCodeGen_compile(LLVMCodeGen *gen, AstNode *ast, Scope *globalScope) {
  return LLVMCodeGen_compile_impl(gen, ast, globalScope);
}
