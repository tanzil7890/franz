#include "llvm_control_flow.h"
#include <stdio.h>
#include <string.h>

/**
 *  Control Flow Helper Functions
 *
 * Implements when/unless as convenience wrappers around if:
 * - when:   Execute action if condition true (sugar for: if cond action)
 * - unless: Execute action if condition false (sugar for: if (not cond) action)
 */

// Forward declarations
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);
extern LLVMValueRef LLVMCodeGen_compileIf(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile when statement
 *
 * Syntax: (when condition action)
 *
 * Behavior:
 * - If condition is true (non-zero), execute action
 * - If condition is false (zero), return 0
 * - Equivalent to: (if condition action)
 *
 * Example:
 *   (when debug {(println "Debug mode ON")})
 *   (when (greater_than x 0) {<- x})
 *
 * Returns: Action result if condition true, 0 otherwise
 *
 * Implementation:
 * Rewrites to if statement with optional else:
 *   (when cond action) → (if cond action)
 */
LLVMValueRef LLVMCodeGen_compileWhen(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: when requires exactly 2 arguments (condition action) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Rewrite as if statement: (when cond action) → (if cond action)
  // The if compiler already supports optional else (returns 0)
  return LLVMCodeGen_compileIf(gen, node);
}

/**
 * Compile unless statement
 *
 * Syntax: (unless condition action)
 *
 * Behavior:
 * - If condition is false (zero), execute action
 * - If condition is true (non-zero), return 0
 * - Equivalent to: (if (not condition) action)
 *
 * Example:
 *   (unless production {(run_tests)})
 *   (unless (is x 0) {<- (divide 10 x)})
 *
 * Returns: Action result if condition false, 0 otherwise
 *
 * Implementation:
 * Inverts condition and calls if:
 *   (unless cond action) → (if (not cond) action)
 */
LLVMValueRef LLVMCodeGen_compileUnless(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: unless requires exactly 2 arguments (condition action) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  AstNode *condNode = node->children[0];
  AstNode *actionNode = node->children[1];

  // Step 1: Compile condition
  LLVMValueRef condValue = LLVMCodeGen_compileNode(gen, condNode);
  if (!condValue) {
    fprintf(stderr, "ERROR: Failed to compile unless condition at line %d\n", node->lineNumber);
    return NULL;
  }

  // Step 2: Invert condition (NOT logic)
  // Any non-zero value is true, zero is false
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);

  // Check if condition is zero
  LLVMValueRef isZero = LLVMBuildICmp(gen->builder, LLVMIntEQ, condValue, zero, "is_zero");
  // Convert i1 to i64
  LLVMValueRef invertedCond = LLVMBuildZExt(gen->builder, isZero, gen->intType, "inverted_cond");

  // Step 3: Create basic blocks
  LLVMBasicBlockRef actionBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "unless_action");
  LLVMBasicBlockRef skipBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "unless_skip");
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "unless_merge");

  // Step 4: Branch on inverted condition
  LLVMValueRef branch_cond = LLVMBuildICmp(gen->builder, LLVMIntNE, invertedCond, zero, "unless_cond");
  LLVMBuildCondBr(gen->builder, branch_cond, actionBlock, skipBlock);

  // Step 5: Compile action block
  LLVMPositionBuilderAtEnd(gen->builder, actionBlock);

  // Check if actionNode is a block or direct expression
  LLVMValueRef actionValue;
  if (actionNode->opcode == OP_FUNCTION) {
    // It's a block, compile as statements
    actionValue = LLVMCodeGen_compileBlockAsStatements(gen, actionNode);
  } else {
    // Direct expression
    actionValue = LLVMCodeGen_compileNode(gen, actionNode);
  }

  if (!actionValue) {
    fprintf(stderr, "ERROR: Failed to compile unless action at line %d\n", actionNode->lineNumber);
    return NULL;
  }

  LLVMBasicBlockRef actionEndBlock = LLVMGetInsertBlock(gen->builder);
  if (!LLVMGetBasicBlockTerminator(actionEndBlock)) {
    LLVMBuildBr(gen->builder, mergeBlock);
  }

  // Step 6: Skip block (condition was true, return 0)
  LLVMPositionBuilderAtEnd(gen->builder, skipBlock);
  LLVMValueRef skipValue = zero;
  LLVMBuildBr(gen->builder, mergeBlock);

  // Step 7: Merge block with PHI
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, gen->intType, "unless_result");

  LLVMValueRef incomingValues[] = {actionValue, skipValue};
  LLVMBasicBlockRef incomingBlocks[] = {actionEndBlock, skipBlock};
  LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);

  return phi;
}
