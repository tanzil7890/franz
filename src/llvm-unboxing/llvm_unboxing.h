#ifndef LLVM_UNBOXING_H
#define LLVM_UNBOXING_H

#include "../llvm-codegen/llvm_codegen.h"
#include <llvm-c/Core.h>

/**
 *  Auto-Unboxing for List Operations
 *
 * This module provides automatic unboxing of Generic* values to native types.
 * When list operations (head, tail, nth) return Generic* pointers, these
 * functions automatically extract the native value based on runtime type.
 *
 * Strategy: Insert runtime unboxing calls when Generic* is used in contexts
 * that expect native types (arithmetic, comparisons, etc.)
 */

/**
 * Check if a value is a Generic* pointer and needs unboxing
 * Returns 1 if value is Generic*, 0 otherwise
 */
int LLVMUnboxing_needsUnboxing(LLVMCodeGen *gen, LLVMValueRef value, AstNode *node);

/**
 * Automatically unbox a Generic* value to the expected native type
 * This inserts a runtime call to check the Generic's type and extract the value
 *
 * Returns: The unboxed native value (i64, double, or i8*)
 */
LLVMValueRef LLVMUnboxing_autoUnbox(LLVMCodeGen *gen, LLVMValueRef genericPtr,
                                     AstNode *node, LLVMTypeRef expectedType);

/**
 * Try to unbox a value for arithmetic operations
 * If the value is Generic*, unbox it to i64 or double
 * Otherwise return the value unchanged
 */
LLVMValueRef LLVMUnboxing_unboxForArithmetic(LLVMCodeGen *gen, LLVMValueRef value, AstNode *node);

#endif // LLVM_UNBOXING_H
