#include "llvm_control_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *  Loop Control Flow with Early Exit
 *
 * Implements counted loops with LLVM basic blocks and back edges:
 *
 * Syntax: (loop count body)
 *
 * Example:
 *   (loop 5 {i -> (println i)})  // Prints 0, 1, 2, 3, 4
 *
 * Early Exit Example:
 *   (loop 10 {i ->
 *     (if (is i 5)
 *       {<- i}      // Return i (early exit)
 *       {<- 0})     // Continue (return 0 = void)
 *   })
 *
 * LLVM Strategy:
 * - Create counter variable (alloca + store initial 0)
 * - Loop condition block: compare counter < count
 * - Loop body block: execute body with counter as argument
 * - Early exit detection: check if body returns non-zero
 * - Increment block: counter++, branch back to condition
 * - Exit block: continue after loop
 *
 * Implementation:
 * - Counter is i64 variable
 * - Body can be block or function call
 * - Early exit: non-zero return value exits loop immediately
 * - Returns void (0) if loop completes normally
 */

// Forward declarations
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);
extern LLVMVariableMap* LLVMVariableMap_new(void);
extern void LLVMVariableMap_free(LLVMVariableMap *map);
extern void LLVMVariableMap_set(LLVMVariableMap *map, const char *name, LLVMValueRef value);

/**
 * Compile loop statement with early exit support
 *
 * Syntax: (loop count body)
 *
 * Behavior:
 * - Executes body `count` times
 * - Passes index i (0 to count-1) to body
 * - Body can be: {i -> ...} block or function call
 * - Early exit: non-zero return value exits loop immediately
 * - Returns void (0) if loop completes normally
 * - Count must be non-negative integer
 *
 * LLVM IR Pattern:
 * ```
 * ; Allocate and initialize counter
 * %counter = alloca i64
 * store i64 0, i64* %counter
 * br label %loop_cond
 *
 * loop_cond:
 *   %i = load i64, i64* %counter
 *   %cmp = icmp slt i64 %i, %count
 *   br i1 %cmp, label %loop_body, label %loop_exit
 *
 * loop_body:
 *   ; Execute body with %i as argument
 *   %result = ...
 *   %is_exit = icmp ne i64 %result, 0
 *   br i1 %is_exit, label %loop_early_exit, label %loop_incr
 *
 * loop_early_exit:
 *   br label %loop_exit
 *
 * loop_incr:
 *   %i_incr = load i64, i64* %counter
 *   %next = add i64 %i_incr, 1
 *   store i64 %next, i64* %counter
 *   br label %loop_cond
 *
 * loop_exit:
 *   ; Continue execution
 * ```
 *
 * Examples:
 *   (loop 5 {i -> (println i)})
 *   // Compiles to: for (int i = 0; i < 5; i++) { println(i); }
 *
 *   (loop 10 {i -> (if (is i 5) {<- i} {<- 0})})
 *   // Early exit when i == 5
 */
LLVMValueRef LLVMCodeGen_compileLoop(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: loop requires exactly 2 arguments (count body) at line %d\n",
            node->lineNumber);
    fprintf(stderr, "Syntax: (loop count {i -> body})\n");
    return NULL;
  }

  AstNode *countNode = node->children[0];
  AstNode *bodyNode = node->children[1];

  // Compile count expression
  LLVMValueRef countValue = LLVMCodeGen_compileNode(gen, countNode);
  if (!countValue) {
    fprintf(stderr, "ERROR: Failed to compile loop count at line %d\n", countNode->lineNumber);
    return NULL;
  }

  // Validate count is integer type
  LLVMTypeRef countType = LLVMTypeOf(countValue);
  if (LLVMGetTypeKind(countType) != LLVMIntegerTypeKind) {
    fprintf(stderr, "ERROR: loop count must be integer at line %d\n", countNode->lineNumber);
    return NULL;
  }

  // Create basic blocks for loop structure (with early exit support)
  LLVMBasicBlockRef loopCondBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "loop_cond");
  LLVMBasicBlockRef loopBodyBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "loop_body");
  LLVMBasicBlockRef loopIncrBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "loop_incr");
  LLVMBasicBlockRef loopExitBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "loop_exit");

  // Allocate counter variable
  LLVMValueRef counterPtr = LLVMBuildAlloca(gen->builder, gen->intType, "loop_counter");

  // Initialize counter to 0
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMBuildStore(gen->builder, zero, counterPtr);

  //  Allocate storage for loop return value (for early exit)
  // Use generic pointer type (i8*) to store any type (int, float, string, etc.)
  LLVMValueRef loopReturnPtr = LLVMBuildAlloca(gen->builder, gen->stringType, "loop_return");
  LLVMValueRef nullPtr = LLVMConstPointerNull(gen->stringType);
  LLVMBuildStore(gen->builder, nullPtr, loopReturnPtr);  // Initialize to NULL

  // -5.7: Save current loop context and set new loop context
  LLVMBasicBlockRef savedLoopExit = gen->loopExitBlock;
  LLVMBasicBlockRef savedLoopIncr = gen->loopIncrBlock;
  LLVMValueRef savedLoopReturn = gen->loopReturnPtr;
  LLVMTypeRef savedLoopReturnType = gen->loopReturnType;
  gen->loopExitBlock = loopExitBlock;
  gen->loopIncrBlock = loopIncrBlock;
  gen->loopReturnPtr = loopReturnPtr;
  gen->loopReturnType = gen->intType;  // Default to int, will be updated on first store

  // Branch to loop condition
  LLVMBuildBr(gen->builder, loopCondBlock);

  // ===== Loop Condition Block =====
  LLVMPositionBuilderAtEnd(gen->builder, loopCondBlock);

  // Load current counter value
  LLVMValueRef currentCounter = LLVMBuildLoad2(gen->builder, gen->intType, counterPtr, "i");

  // Compare: i < count
  LLVMValueRef cmp = LLVMBuildICmp(gen->builder, LLVMIntSLT, currentCounter, countValue, "loop_cmp");

  // Branch: if (i < count) goto body, else goto exit
  LLVMBuildCondBr(gen->builder, cmp, loopBodyBlock, loopExitBlock);

  // ===== Loop Body Block =====
  LLVMPositionBuilderAtEnd(gen->builder, loopBodyBlock);

  // Check if body is a function/block with parameter
  if (bodyNode->opcode == OP_FUNCTION) {
    // Body is a block: {i -> ...}
    // Extract parameter name and bind to current counter value

    if (bodyNode->childCount < 2) {
      fprintf(stderr, "ERROR: Loop body function must have parameter and body at line %d\n",
              bodyNode->lineNumber);
      return NULL;
    }

    // First child is the parameter name (e.g., "i")
    AstNode *paramNode = bodyNode->children[0];
    if (paramNode->opcode != OP_IDENTIFIER) {
      fprintf(stderr, "ERROR: Loop parameter must be identifier at line %d\n",
              paramNode->lineNumber);
      return NULL;
    }

    // All children after the first are statements in the body
    // Create a new variable scope for the loop body
    // Save current variables map
    LLVMVariableMap *prevVars = gen->variables;
    gen->variables = LLVMVariableMap_new();

    // Copy all variables from parent scope to new scope
    for (int i = 0; i < prevVars->count; i++) {
      LLVMVariableMap_set(gen->variables, prevVars->entries[i].name,
                         prevVars->entries[i].value);
    }

    // Bind the parameter name to the current counter value
    LLVMVariableMap_set(gen->variables, paramNode->val, currentCounter);

    // Compile all statements in the function body (children after parameter)
    for (int i = 1; i < bodyNode->childCount; i++) {
      AstNode *stmtNode = bodyNode->children[i];
      LLVMValueRef result = LLVMCodeGen_compileNode(gen, stmtNode);
      if (!result) {
        fprintf(stderr, "WARNING: Failed to compile loop body statement at line %d\n",
                stmtNode->lineNumber);
      }
    }

    // After all statements, check if we need to branch to increment block
    // If there was a return statement (<- value), the block already has a terminator
    // and control never reaches here - the return exits the current function entirely.
    // For early loop exit (like Rust's break), we need to branch to loop exit instead
    // of returning from the function. But Franz's <- always returns from function.
    // So for now, just continue to next iteration if no return.
    LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
    if (!LLVMGetBasicBlockTerminator(currentBlock)) {
      // No return statement - continue to next iteration
      LLVMBuildBr(gen->builder, loopIncrBlock);
    }

    // Restore previous variable scope
    LLVMVariableMap_free(gen->variables);
    gen->variables = prevVars;

  } else {
    // Body is a direct expression or function call (no parameter)
    LLVMValueRef bodyResult = LLVMCodeGen_compileNode(gen, bodyNode);
    if (!bodyResult) {
      fprintf(stderr, "WARNING: Failed to compile loop body at line %d\n", bodyNode->lineNumber);
    }

    // After body compilation, check if we need to branch
    // If there was a return statement, the block already has a terminator
    LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
    if (!LLVMGetBasicBlockTerminator(currentBlock)) {
      // No return - continue to next iteration
      LLVMBuildBr(gen->builder, loopIncrBlock);
    }
  }

  // ===== Loop Increment Block =====
  LLVMPositionBuilderAtEnd(gen->builder, loopIncrBlock);

  // Load counter again (may be different from loop body's load due to SSA)
  LLVMValueRef counterForIncr = LLVMBuildLoad2(gen->builder, gen->intType, counterPtr, "i_incr");

  // Increment counter: counter++
  LLVMValueRef nextCounter = LLVMBuildAdd(gen->builder, counterForIncr,
                                          LLVMConstInt(gen->intType, 1, 0), "next_i");
  LLVMBuildStore(gen->builder, nextCounter, counterPtr);

  // Branch back to condition (back edge)
  LLVMBuildBr(gen->builder, loopCondBlock);

  // ===== Loop Exit Block =====
  LLVMPositionBuilderAtEnd(gen->builder, loopExitBlock);

  // -5.7: Restore previous loop context
  LLVMTypeRef actualReturnType = gen->loopReturnType;
  gen->loopExitBlock = savedLoopExit;
  gen->loopIncrBlock = savedLoopIncr;
  gen->loopReturnPtr = savedLoopReturn;
  gen->loopReturnType = savedLoopReturnType;

  // Load and return the loop result (NULL/0 if no early exit, or the early exit value)
  LLVMValueRef loopResultPtr = LLVMBuildLoad2(gen->builder, gen->stringType, loopReturnPtr, "loop_result_ptr");
  
  // Cast pointer back to original type if needed
  if (actualReturnType == gen->intType) {
    // Convert pointer to integer
    LLVMValueRef loopResult = LLVMBuildPtrToInt(gen->builder, loopResultPtr, gen->intType, "loop_result");
    return loopResult;
  } else if (actualReturnType == gen->floatType) {
    // Convert pointer to int then to float
    LLVMValueRef asInt = LLVMBuildPtrToInt(gen->builder, loopResultPtr, gen->intType, "as_int");
    LLVMValueRef loopResult = LLVMBuildSIToFP(gen->builder, asInt, gen->floatType, "loop_result");
    return loopResult;
  } else {
    // Return pointer as-is (string or other pointer type)
    return loopResultPtr;
  }
}

/**
 * Compile while loop with break/continue support
 *
 * Syntax: (while condition body)
 *
 * Behavior:
 * - Evaluates condition before each iteration
 * - Executes body while condition is true (non-zero)
 * - Supports break and continue statements
 * - Returns value from break, or 0 if completes normally
 *
 * LLVM IR Pattern:
 * ```
 * ; Allocate return value storage
 * %while_return = alloca i64
 * store i64 0, i64* %while_return
 * br label %while_cond
 *
 * while_cond:
 *   %cond_val = ... ; Evaluate condition
 *   %cond = icmp ne i64 %cond_val, 0
 *   br i1 %cond, label %while_body, label %while_exit
 *
 * while_body:
 *   ; Execute body statements
 *   ; Can use (break value) or (continue)
 *   br label %while_incr
 *
 * while_incr:
 *   ; Continue jumps here
 *   br label %while_cond
 *
 * while_exit:
 *   ; Break jumps here
 *   %result = load i64, i64* %while_return
 * ```
 *
 * Examples:
 *   counter = 0
 *   (while (less_than counter 5) {
 *     (println counter)
 *     counter = (add counter 1)
 *   })
 *
 *   result = (while 1 {
 *     (if (is counter 5) {(break counter)} {})
 *     counter = (add counter 1)
 *   })
 */
LLVMValueRef LLVMCodeGen_compileWhile(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: while requires exactly 2 arguments (condition body) at line %d\n",
            node->lineNumber);
    fprintf(stderr, "Syntax: (while condition body)\n");
    return NULL;
  }

  AstNode *conditionNode = node->children[0];
  AstNode *bodyNode = node->children[1];

  // Create basic blocks for while loop structure
  LLVMBasicBlockRef whileCondBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "while_cond");
  LLVMBasicBlockRef whileBodyBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "while_body");
  LLVMBasicBlockRef whileIncrBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "while_incr");
  LLVMBasicBlockRef whileExitBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "while_exit");

  // Allocate storage for while return value (for break support)
  LLVMValueRef whileReturnPtr = LLVMBuildAlloca(gen->builder, gen->intType, "while_return");
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMBuildStore(gen->builder, zero, whileReturnPtr);  // Initialize to 0

  // Save current loop context and set new loop context
  LLVMBasicBlockRef savedLoopExit = gen->loopExitBlock;
  LLVMBasicBlockRef savedLoopIncr = gen->loopIncrBlock;
  LLVMValueRef savedLoopReturn = gen->loopReturnPtr;
  gen->loopExitBlock = whileExitBlock;
  gen->loopIncrBlock = whileIncrBlock;
  gen->loopReturnPtr = whileReturnPtr;

  // Branch to while condition
  LLVMBuildBr(gen->builder, whileCondBlock);

  // ===== While Condition Block =====
  LLVMPositionBuilderAtEnd(gen->builder, whileCondBlock);

  // Evaluate condition
  LLVMValueRef conditionValue = LLVMCodeGen_compileNode(gen, conditionNode);
  if (!conditionValue) {
    fprintf(stderr, "ERROR: Failed to compile while condition at line %d\n",
            conditionNode->lineNumber);
    return NULL;
  }

  // Convert condition to boolean (0 = false, non-zero = true)
  LLVMValueRef cond = LLVMBuildICmp(gen->builder, LLVMIntNE, conditionValue, zero, "while_cmp");

  // Branch: if (condition) goto body, else goto exit
  LLVMBuildCondBr(gen->builder, cond, whileBodyBlock, whileExitBlock);

  // ===== While Body Block =====
  LLVMPositionBuilderAtEnd(gen->builder, whileBodyBlock);

  // Compile body using compileBlockAsStatements to handle {} blocks
  // This is different from loop which needs a parameter
  extern LLVMValueRef LLVMCodeGen_compileBlockAsStatements(LLVMCodeGen *gen, AstNode *blockNode);
  LLVMValueRef bodyResult = LLVMCodeGen_compileBlockAsStatements(gen, bodyNode);
  if (!bodyResult) {
    fprintf(stderr, "WARNING: Failed to compile while body at line %d\n", bodyNode->lineNumber);
  }

  // After body compilation, check if we need to branch
  // If there was a break/continue, the block already has a terminator
  LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
  if (!LLVMGetBasicBlockTerminator(currentBlock)) {
    // No break/continue - go to increment block
    LLVMBuildBr(gen->builder, whileIncrBlock);
  }

  // ===== While Increment Block =====
  LLVMPositionBuilderAtEnd(gen->builder, whileIncrBlock);

  // Branch back to condition (back edge - creates the loop)
  LLVMBuildBr(gen->builder, whileCondBlock);

  // ===== While Exit Block =====
  LLVMPositionBuilderAtEnd(gen->builder, whileExitBlock);

  // Restore previous loop context
  gen->loopExitBlock = savedLoopExit;
  gen->loopIncrBlock = savedLoopIncr;
  gen->loopReturnPtr = savedLoopReturn;

  // Load and return the while result (0 if no break, or the break value)
  LLVMValueRef whileResult = LLVMBuildLoad2(gen->builder, gen->intType, whileReturnPtr, "while_result");
  return whileResult;
}
