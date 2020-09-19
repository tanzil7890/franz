#ifndef LLVM_COMPARISONS_H
#define LLVM_COMPARISONS_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

//  Comparison Operators for LLVM Native Compilation
// Implements: is, less_than, greater_than

/**
 * Compile equality comparison (is)
 * Supports: integers, floats, strings
 * Returns: 1 if equal, 0 if not equal
 */
LLVMValueRef LLVMCodeGen_compileIs(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile less-than comparison
 * Supports: integers, floats (automatic type promotion)
 * Returns: 1 if left < right, 0 otherwise
 */
LLVMValueRef LLVMCodeGen_compileLessThan(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile greater-than comparison
 * Supports: integers, floats (automatic type promotion)
 * Returns: 1 if left > right, 0 otherwise
 */
LLVMValueRef LLVMCodeGen_compileGreaterThan(LLVMCodeGen *gen, AstNode *node);

#endif
