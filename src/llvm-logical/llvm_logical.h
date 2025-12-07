#ifndef LLVM_LOGICAL_H
#define LLVM_LOGICAL_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

//  Logical Operators for LLVM Native Compilation
// Implements: not, and, or

/**
 * Compile logical NOT
 * Inverts a boolean value (0 → 1, 1 → 0)
 * Syntax: (not value)
 * Returns: 1 if value is 0, 0 otherwise
 */
LLVMValueRef LLVMCodeGen_compileNot(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile logical AND
 * Returns 1 if all arguments are non-zero (truthy), 0 otherwise
 * Syntax: (and val1 val2 ...)
 * Variadic: Accepts 2+ arguments
 * Returns: 1 if all args are truthy, 0 otherwise
 */
LLVMValueRef LLVMCodeGen_compileAnd(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile logical OR
 * Returns 1 if any argument is non-zero (truthy), 0 otherwise
 * Syntax: (or val1 val2 ...)
 * Variadic: Accepts 2+ arguments
 * Returns: 1 if any arg is truthy, 0 otherwise
 */
LLVMValueRef LLVMCodeGen_compileOr(LLVMCodeGen *gen, AstNode *node);

// ============================================================================
//  Short-Circuit Evaluation (Industry Standard)
// ============================================================================

/**
 * Compile logical AND with short-circuit evaluation
 * Stops evaluating as soon as a false (0) value is found
 * Syntax: (and val1 val2 ...)
 * Behavior:
 *   - (and 0 expensive_call) → 0 (expensive_call NOT evaluated)
 *   - (and 1 1 1) → 1 (all evaluated because all true)
 * Performance: C/Rust equivalent (same LLVM IR pattern)
 */
LLVMValueRef LLVMCodeGen_compileAndShortCircuit(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile logical OR with short-circuit evaluation
 * Stops evaluating as soon as a true (non-zero) value is found
 * Syntax: (or val1 val2 ...)
 * Behavior:
 *   - (or 1 expensive_call) → 1 (expensive_call NOT evaluated)
 *   - (or 0 0 0) → 0 (all evaluated because all false)
 * Performance: C/Rust equivalent (same LLVM IR pattern)
 */
LLVMValueRef LLVMCodeGen_compileOrShortCircuit(LLVMCodeGen *gen, AstNode *node);

#endif
