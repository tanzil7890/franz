#include "llvm_logical.h"
#include <stdio.h>
#include <string.h>

/**
 *  Logical Operators for LLVM Native Compilation
 *
 * This module implements three logical operators:
 * 1. not - Logical NOT (unary)
 * 2. and - Logical AND (variadic, 2+ arguments)
 * 3. or  - Logical OR (variadic, 2+ arguments)
 *
 * All functions work with integers as booleans (0 = false, non-zero = true)
 * and return TYPE_INT (0 or 1).
 */

// Forward declaration for node compilation
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile logical NOT
 *
 * Syntax: (not value)
 *
 * Behavior:
 * - (not 0) → 1 (0 is false, so NOT returns true)
 * - (not 1) → 0 (1 is true, so NOT returns false)
 * - (not 5) → 0 (5 is truthy, so NOT returns false)
 * - (not -1) → 0 (any non-zero is truthy)
 *
 * Returns: i64 (0 or 1)
 *
 * Implementation:
 * Uses LLVM XOR with 1 to flip the least significant bit:
 * - If value is 0: XOR with 1 = 1
 * - If value is 1: XOR with 1 = 0
 * - If value is other: First normalize to 0/1 using icmp ne 0, then XOR
 *
 * Note: To handle values > 1 correctly, we first convert to boolean:
 * 1. Compare value != 0 to get i1 boolean
 * 2. Zero-extend i1 to i64 to get 0 or 1
 * 3. XOR with 1 to invert
 */
LLVMValueRef LLVMCodeGen_compileNot(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: not requires exactly 1 argument at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile operand
  LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!value) {
    return NULL;
  }

  LLVMTypeRef valueType = LLVMTypeOf(value);

  // Only support integers for logical operations
  if (valueType != gen->intType) {
    fprintf(stderr, "ERROR: not requires integer argument at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Step 1: Convert to boolean (0 or 1)
  // Compare: value != 0
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef isNonZero = LLVMBuildICmp(gen->builder, LLVMIntNE, value, zero, "is_nonzero");

  // Step 2: Zero-extend i1 to i64
  LLVMValueRef boolAsInt = LLVMBuildZExt(gen->builder, isNonZero, gen->intType, "bool_to_int");

  // Step 3: XOR with 1 to invert (0 → 1, 1 → 0)
  LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);
  LLVMValueRef result = LLVMBuildXor(gen->builder, boolAsInt, one, "not");

  return result;
}

/**
 * Compile logical AND
 *
 * Syntax: (and val1 val2 ...)
 *
 * Behavior:
 * - (and 1 1) → 1 (all truthy)
 * - (and 1 0) → 0 (one is falsy)
 * - (and 1 1 1) → 1 (all truthy, variadic)
 * - (and 5 3 1) → 1 (all non-zero = truthy)
 * - (and 1 0 1) → 0 (short-circuit: stops at first 0)
 *
 * Returns: i64 (0 or 1)
 *
 * Implementation:
 * Iteratively combines arguments with logical AND:
 * 1. Start with result = 1 (true)
 * 2. For each argument:
 *    a. Convert to boolean (value != 0)
 *    b. AND with current result
 * 3. Return final result
 *
 * Note: Variadic - accepts 2 or more arguments
 */
LLVMValueRef LLVMCodeGen_compileAnd(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count (minimum 2)
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: and requires at least 2 arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Initialize result to 1 (true)
  LLVMValueRef result = LLVMConstInt(gen->intType, 1, 0);
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);

  // Process each argument
  for (int i = 0; i < node->childCount; i++) {
    // Compile argument
    LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!value) {
      return NULL;
    }

    LLVMTypeRef valueType = LLVMTypeOf(value);

    // Only support integers
    if (valueType != gen->intType) {
      fprintf(stderr, "ERROR: and requires integer arguments at line %d\n",
              node->lineNumber);
      return NULL;
    }

    // Convert to boolean: value != 0
    LLVMValueRef isNonZero = LLVMBuildICmp(gen->builder, LLVMIntNE, value, zero, "is_nonzero");

    // Zero-extend i1 to i64
    LLVMValueRef boolAsInt = LLVMBuildZExt(gen->builder, isNonZero, gen->intType, "bool_to_int");

    // AND with current result
    // We use multiplication since we're working with 0/1 values: AND(a,b) = a * b
    result = LLVMBuildMul(gen->builder, result, boolAsInt, "and");
  }

  return result;
}

/**
 * Compile logical OR
 *
 * Syntax: (or val1 val2 ...)
 *
 * Behavior:
 * - (or 0 0) → 0 (all falsy)
 * - (or 0 1) → 1 (one is truthy)
 * - (or 0 0 1) → 1 (at least one truthy, variadic)
 * - (or 5 0 0) → 1 (5 is non-zero = truthy)
 * - (or 0 0 0) → 0 (all falsy)
 *
 * Returns: i64 (0 or 1)
 *
 * Implementation:
 * Iteratively combines arguments with logical OR:
 * 1. Start with result = 0 (false)
 * 2. For each argument:
 *    a. Convert to boolean (value != 0)
 *    b. OR with current result
 * 3. Return final result
 *
 * Note: Variadic - accepts 2 or more arguments
 */
LLVMValueRef LLVMCodeGen_compileOr(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count (minimum 2)
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: or requires at least 2 arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Initialize result to 0 (false)
  LLVMValueRef result = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);

  // Process each argument
  for (int i = 0; i < node->childCount; i++) {
    // Compile argument
    LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!value) {
      return NULL;
    }

    LLVMTypeRef valueType = LLVMTypeOf(value);

    // Only support integers
    if (valueType != gen->intType) {
      fprintf(stderr, "ERROR: or requires integer arguments at line %d\n",
              node->lineNumber);
      return NULL;
    }

    // Convert to boolean: value != 0
    LLVMValueRef isNonZero = LLVMBuildICmp(gen->builder, LLVMIntNE, value, zero, "is_nonzero");

    // Zero-extend i1 to i64
    LLVMValueRef boolAsInt = LLVMBuildZExt(gen->builder, isNonZero, gen->intType, "bool_to_int");

    // OR with current result
    // For boolean OR with 0/1 values: OR(a,b) = a + b - (a * b)
    // But simpler: if either is 1, result should be 1
    // Use: result = result | boolAsInt
    result = LLVMBuildOr(gen->builder, result, boolAsInt, "or");
  }

  return result;
}
