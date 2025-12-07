#include "llvm_control_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *  Cond Chains
 *
 * Implements pattern-matching style conditional chains similar to Lisp/Scheme:
 *
 * Syntax: (cond (test1 result1) (test2 result2) ... (else default))
 *
 * Example:
 *   (cond
 *     ((greater_than x 10) "big")
 *     ((greater_than x 5) "medium")
 *     (else "small"))
 *
 * LLVM Strategy:
 * - Chain of if-else blocks
 * - Each clause creates: test block → result block OR next test
 * - Else clause provides default value
 * - All paths converge at final merge block with PHI node
 *
 * Implementation:
 * - Compile each test condition sequentially
 * - Branch to result block if true, next test if false
 * - Collect all result values for PHI node
 * - Type promotion handled automatically (INT→FLOAT)
 */

// Forward declarations
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);

/**
 * Check if a clause is an else clause
 *
 * Else clause syntax: (else value)
 * - First child is identifier with name "else"
 * - Second child is the default value
 */
static int isElseClause(AstNode *clauseNode) {
  if (!clauseNode || clauseNode->opcode != OP_APPLICATION) {
    return 0;
  }

  if (clauseNode->childCount != 2) {
    return 0;
  }

  AstNode *first = clauseNode->children[0];
  if (first->opcode != OP_IDENTIFIER) {
    return 0;
  }

  return (strcmp(first->val, "else") == 0);
}

/**
 * Compile cond chains
 *
 * Syntax: (cond (test1 result1) (test2 result2) ... (else default))
 *
 * Behavior:
 * - Evaluates each test condition in order
 * - Returns result from first true condition
 * - If no conditions match, returns else clause value
 * - If no else clause and no match, returns 0
 *
 * LLVM IR Pattern:
 * ```
 * ; Test first condition
 * %test1 = ...
 * %cond1 = icmp ne i64 %test1, 0
 * br i1 %cond1, label %result1, label %test2_block
 *
 * result1:
 *   %val1 = ...
 *   br label %merge
 *
 * test2_block:
 *   %test2 = ...
 *   %cond2 = icmp ne i64 %test2, 0
 *   br i1 %cond2, label %result2, label %else_block
 *
 * result2:
 *   %val2 = ...
 *   br label %merge
 *
 * else_block:
 *   %else_val = ...
 *   br label %merge
 *
 * merge:
 *   %result = phi i64 [ %val1, %result1 ], [ %val2, %result2 ], [ %else_val, %else_block ]
 * ```
 */
LLVMValueRef LLVMCodeGen_compileCond(LLVMCodeGen *gen, AstNode *node) {
  // Validate minimum arguments (at least 1 clause)
  if (node->childCount < 1) {
    fprintf(stderr, "ERROR: cond requires at least 1 clause at line %d\n", node->lineNumber);
    fprintf(stderr, "Syntax: (cond (test1 result1) (test2 result2) ... (else default))\n");
    return NULL;
  }

  // Create final merge block where all paths converge
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "cond_merge");

  // Track incoming values and blocks for PHI node
  // Allocate space for all clauses + 1 for potential fallthrough
  LLVMValueRef *incomingValues = malloc(sizeof(LLVMValueRef) * (node->childCount + 1));
  LLVMBasicBlockRef *incomingBlocks = malloc(sizeof(LLVMBasicBlockRef) * (node->childCount + 1));
  int incomingCount = 0;

  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMBasicBlockRef currentTestBlock = LLVMGetInsertBlock(gen->builder);
  int hasElse = 0;

  // Process each clause
  for (int i = 0; i < node->childCount; i++) {
    AstNode *clause = node->children[i];

    // Validate clause structure
    if (clause->opcode != OP_APPLICATION) {
      fprintf(stderr, "ERROR: cond clause must be an application (test result) at line %d\n",
              clause->lineNumber);
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    // Check if this is the else clause
    if (isElseClause(clause)) {
      hasElse = 1;

      // Position at current test block (last failed test leads here)
      LLVMPositionBuilderAtEnd(gen->builder, currentTestBlock);

      // Compile else value
      AstNode *elseValue = clause->children[1];
      LLVMValueRef elseResult = LLVMCodeGen_compileNode(gen, elseValue);
      if (!elseResult) {
        fprintf(stderr, "ERROR: Failed to compile else value at line %d\n", elseValue->lineNumber);
        free(incomingValues);
        free(incomingBlocks);
        return NULL;
      }

      // Branch to merge
      LLVMBuildBr(gen->builder, mergeBlock);
      incomingValues[incomingCount] = elseResult;
      incomingBlocks[incomingCount] = currentTestBlock;
      incomingCount++;

      // Else must be last clause
      if (i != node->childCount - 1) {
        fprintf(stderr, "WARNING: else clause should be last in cond at line %d\n",
                clause->lineNumber);
      }

      break;
    }

    // Regular clause: (test result)
    if (clause->childCount != 2) {
      fprintf(stderr, "ERROR: cond clause requires exactly 2 elements (test result) at line %d\n",
              clause->lineNumber);
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    AstNode *testNode = clause->children[0];
    AstNode *resultNode = clause->children[1];

    // Position at current test block
    LLVMPositionBuilderAtEnd(gen->builder, currentTestBlock);

    // Compile test condition
    LLVMValueRef testValue = LLVMCodeGen_compileNode(gen, testNode);
    if (!testValue) {
      fprintf(stderr, "ERROR: Failed to compile cond test at line %d\n", testNode->lineNumber);
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    // Convert test to i1 boolean
    LLVMValueRef testCond = LLVMBuildICmp(gen->builder, LLVMIntNE, testValue, zero, "cond_test");

    // Create result block for this clause
    char resultBlockName[64];
    snprintf(resultBlockName, sizeof(resultBlockName), "cond_result_%d", i);
    LLVMBasicBlockRef resultBlock = LLVMAppendBasicBlockInContext(
        gen->context, gen->currentFunction, resultBlockName);

    // Create next test block OR fallthrough block
    LLVMBasicBlockRef nextTestBlock;
    if (i == node->childCount - 1) {
      // Last clause - create fallthrough block for no-match case
      nextTestBlock = LLVMAppendBasicBlockInContext(
          gen->context, gen->currentFunction, "cond_fallthrough");
    } else {
      // Not last clause - create next test block
      char nextBlockName[64];
      snprintf(nextBlockName, sizeof(nextBlockName), "cond_test_%d", i + 1);
      nextTestBlock = LLVMAppendBasicBlockInContext(
          gen->context, gen->currentFunction, nextBlockName);
    }

    // Branch: if test true → result block, else → next test block
    LLVMBuildCondBr(gen->builder, testCond, resultBlock, nextTestBlock);

    // Compile result in result block
    LLVMPositionBuilderAtEnd(gen->builder, resultBlock);
    LLVMValueRef resultValue = LLVMCodeGen_compileNode(gen, resultNode);
    if (!resultValue) {
      fprintf(stderr, "ERROR: Failed to compile cond result at line %d\n", resultNode->lineNumber);
      free(incomingValues);
      free(incomingBlocks);
      return NULL;
    }

    // Branch from result block to merge
    LLVMBuildBr(gen->builder, mergeBlock);
    incomingValues[incomingCount] = resultValue;
    incomingBlocks[incomingCount] = resultBlock;
    incomingCount++;

    // Move to next test block
    currentTestBlock = nextTestBlock;
  }

  // If no else clause was provided, add default 0 from last test block
  if (!hasElse) {
    LLVMPositionBuilderAtEnd(gen->builder, currentTestBlock);
    LLVMBuildBr(gen->builder, mergeBlock);
    incomingValues[incomingCount] = zero;
    incomingBlocks[incomingCount] = currentTestBlock;
    incomingCount++;
  }

  // Build PHI node in merge block
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);

  // Determine result type (promote to double if any result is double)
  LLVMTypeRef resultType = gen->intType;
  for (int i = 0; i < incomingCount; i++) {
    LLVMTypeRef valType = LLVMTypeOf(incomingValues[i]);
    if (LLVMGetTypeKind(valType) == LLVMDoubleTypeKind) {
      resultType = gen->floatType;
      break;
    } else if (LLVMGetTypeKind(valType) == LLVMPointerTypeKind) {
      resultType = valType;  // String type
      break;
    }
  }

  // Promote all values to result type if needed
  for (int i = 0; i < incomingCount; i++) {
    LLVMTypeRef valType = LLVMTypeOf(incomingValues[i]);
    LLVMTypeKind valKind = LLVMGetTypeKind(valType);
    LLVMTypeKind resKind = LLVMGetTypeKind(resultType);

    if (valKind != resKind) {
      // Need type promotion
      if (resKind == LLVMDoubleTypeKind && valKind == LLVMIntegerTypeKind) {
        // INT → FLOAT
        // Position builder BEFORE the terminator instruction
        LLVMBasicBlockRef block = incomingBlocks[i];
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(block);

        if (terminator) {
          // Insert before the terminator
          LLVMPositionBuilderBefore(gen->builder, terminator);
        } else {
          // No terminator yet, position at end
          LLVMPositionBuilderAtEnd(gen->builder, block);
        }

        // Create promotion instruction
        LLVMValueRef promoted = LLVMBuildSIToFP(gen->builder, incomingValues[i], gen->floatType, "cond_promote");
        incomingValues[i] = promoted;
      }
    }
  }

  // Build PHI node
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, resultType, "cond_result");
  LLVMAddIncoming(phi, incomingValues, incomingBlocks, incomingCount);

  // Cleanup
  free(incomingValues);
  free(incomingBlocks);

  return phi;
}
