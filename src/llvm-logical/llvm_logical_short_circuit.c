#include "llvm_logical.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *  Short-Circuit Evaluation for Logical Operators
 *
 * This module implements short-circuit evaluation for AND/OR:
 * - AND: Stops at first false value (0)
 * - OR: Stops at first true value (non-zero)
 *
 * Benefits:
 * - Performance: Avoids evaluating unnecessary expressions
 * - Correctness: Essential for guard patterns like (and ptr (is (deref ptr) 0))
 * - Industry standard: Matches C, Rust, Python, JavaScript behavior
 *
 * Implementation Strategy:
 * Uses LLVM basic blocks to implement conditional evaluation:
 * 1. Create blocks for each argument + final block
 * 2. Evaluate argument
 * 3. Branch to next argument or skip to final based on result
 * 4. Use PHI node to merge final result
 */

// Forward declaration for node compilation
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile logical AND with short-circuit evaluation
 *
 * Syntax: (and val1 val2 val3 ...)
 *
 * Short-Circuit Behavior:
 * - (and 0 expensive_call) → 0 (expensive_call NOT evaluated)
 * - (and 1 1 0 expensive_call) → 0 (expensive_call NOT evaluated)
 * - (and 1 1 1) → 1 (all evaluated because all true)
 *
 * LLVM IR Pattern:
 * ```
 * entry:
 *   %arg1 = evaluate first argument
 *   %cond1 = icmp ne %arg1, 0
 *   br i1 %cond1, label %eval2, label %final
 *
 * eval2:
 *   %arg2 = evaluate second argument
 *   %cond2 = icmp ne %arg2, 0
 *   br i1 %cond2, label %eval3, label %final
 *
 * eval3:
 *   %arg3 = evaluate third argument
 *   %cond3 = icmp ne %arg3, 0
 *   br label %final
 *
 * final:
 *   %result = phi i64 [ 0, %entry ], [ 0, %eval2 ], [ %cond3, %eval3 ]
 * ```
 *
 * Returns: i64 (0 or 1)
 */
LLVMValueRef LLVMCodeGen_compileAndShortCircuit(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count (minimum 2)
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: and requires at least 2 arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);

  // Create final block where all paths converge
  LLVMBasicBlockRef finalBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "and_final");

  // Track incoming values and blocks for PHI node
  LLVMValueRef *incomingValues = malloc(sizeof(LLVMValueRef) * (node->childCount + 1));
  LLVMBasicBlockRef *incomingBlocks = malloc(sizeof(LLVMBasicBlockRef) * (node->childCount + 1));
  int incomingCount = 0;

  // Evaluate each argument with short-circuit logic
  for (int i = 0; i < node->childCount; i++) {
    // Get current block
    LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);

    // Compile argument
    LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!value) {
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    LLVMTypeRef valueType = LLVMTypeOf(value);

    // Only support integers
    if (valueType != gen->intType) {
      fprintf(stderr, "ERROR: and requires integer arguments at line %d\n",
              node->lineNumber);
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    // Convert to boolean: value != 0
    LLVMValueRef isNonZero = LLVMBuildICmp(gen->builder, LLVMIntNE, value, zero, "is_nonzero");
    LLVMValueRef boolAsInt = LLVMBuildZExt(gen->builder, isNonZero, gen->intType, "bool_to_int");

    // Check if this is the last argument
    if (i == node->childCount - 1) {
      // Last argument: just branch to final with its value
      LLVMBuildBr(gen->builder, finalBlock);
      incomingValues[incomingCount] = boolAsInt;
      incomingBlocks[incomingCount] = LLVMGetInsertBlock(gen->builder);
      incomingCount++;
    } else {
      // Not last argument: short-circuit if false
      LLVMBasicBlockRef nextBlock = LLVMAppendBasicBlockInContext(
          gen->context, gen->currentFunction, "and_next");

      // If false (0), jump to final. If true (1), evaluate next argument.
      LLVMBuildCondBr(gen->builder, isNonZero, nextBlock, finalBlock);

      // Record this as a potential incoming value (false = 0)
      incomingValues[incomingCount] = zero;
      incomingBlocks[incomingCount] = LLVMGetInsertBlock(gen->builder);
      incomingCount++;

      // Continue building in next block
      LLVMPositionBuilderAtEnd(gen->builder, nextBlock);
    }
  }

  // Build PHI node in final block
  LLVMPositionBuilderAtEnd(gen->builder, finalBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, gen->intType, "and_result");
  LLVMAddIncoming(phi, incomingValues, incomingBlocks, incomingCount);

  free(incomingValues);
  free(incomingBlocks);

  return phi;
}

/**
 * Compile logical OR with short-circuit evaluation
 *
 * Syntax: (or val1 val2 val3 ...)
 *
 * Short-Circuit Behavior:
 * - (or 1 expensive_call) → 1 (expensive_call NOT evaluated)
 * - (or 0 0 1 expensive_call) → 1 (expensive_call NOT evaluated)
 * - (or 0 0 0) → 0 (all evaluated because all false)
 *
 * LLVM IR Pattern:
 * ```
 * entry:
 *   %arg1 = evaluate first argument
 *   %cond1 = icmp ne %arg1, 0
 *   br i1 %cond1, label %final, label %eval2
 *
 * eval2:
 *   %arg2 = evaluate second argument
 *   %cond2 = icmp ne %arg2, 0
 *   br i1 %cond2, label %final, label %eval3
 *
 * eval3:
 *   %arg3 = evaluate third argument
 *   %cond3 = icmp ne %arg3, 0
 *   br label %final
 *
 * final:
 *   %result = phi i64 [ 1, %entry ], [ 1, %eval2 ], [ %cond3, %eval3 ]
 * ```
 *
 * Returns: i64 (0 or 1)
 */
LLVMValueRef LLVMCodeGen_compileOrShortCircuit(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count (minimum 2)
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: or requires at least 2 arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);

  // Create final block where all paths converge
  LLVMBasicBlockRef finalBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "or_final");

  // Track incoming values and blocks for PHI node
  LLVMValueRef *incomingValues = malloc(sizeof(LLVMValueRef) * (node->childCount + 1));
  LLVMBasicBlockRef *incomingBlocks = malloc(sizeof(LLVMBasicBlockRef) * (node->childCount + 1));
  int incomingCount = 0;

  // Evaluate each argument with short-circuit logic
  for (int i = 0; i < node->childCount; i++) {
    // Get current block
    LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);

    // Compile argument
    LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!value) {
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    LLVMTypeRef valueType = LLVMTypeOf(value);

    // Only support integers
    if (valueType != gen->intType) {
      fprintf(stderr, "ERROR: or requires integer arguments at line %d\n",
              node->lineNumber);
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    // Convert to boolean: value != 0
    LLVMValueRef isNonZero = LLVMBuildICmp(gen->builder, LLVMIntNE, value, zero, "is_nonzero");
    LLVMValueRef boolAsInt = LLVMBuildZExt(gen->builder, isNonZero, gen->intType, "bool_to_int");

    // Check if this is the last argument
    if (i == node->childCount - 1) {
      // Last argument: just branch to final with its value
      LLVMBuildBr(gen->builder, finalBlock);
      incomingValues[incomingCount] = boolAsInt;
      incomingBlocks[incomingCount] = LLVMGetInsertBlock(gen->builder);
      incomingCount++;
    } else {
      // Not last argument: short-circuit if true
      LLVMBasicBlockRef nextBlock = LLVMAppendBasicBlockInContext(
          gen->context, gen->currentFunction, "or_next");

      // If true (1), jump to final. If false (0), evaluate next argument.
      LLVMBuildCondBr(gen->builder, isNonZero, finalBlock, nextBlock);

      // Record this as a potential incoming value (true = 1)
      incomingValues[incomingCount] = one;
      incomingBlocks[incomingCount] = LLVMGetInsertBlock(gen->builder);
      incomingCount++;

      // Continue building in next block
      LLVMPositionBuilderAtEnd(gen->builder, nextBlock);
    }
  }

  // Build PHI node in final block
  LLVMPositionBuilderAtEnd(gen->builder, finalBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, gen->intType, "or_result");
  LLVMAddIncoming(phi, incomingValues, incomingBlocks, incomingCount);

  free(incomingValues);
  free(incomingBlocks);

  return phi;
}
