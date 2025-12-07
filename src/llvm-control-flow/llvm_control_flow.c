#include "llvm_control_flow.h"
#include <stdio.h>
#include <string.h>

/**
 *  If Statement for LLVM Native Compilation
 *
 * Implements conditional branching with basic blocks and PHI nodes.
 *
 * Syntax: (if condition then-block else-block)
 *
 * LLVM IR Strategy:
 * 1. Compile condition expression → i64 value
 * 2. Convert i64 to i1 boolean (icmp ne %cond, 0)
 * 3. Create three basic blocks:
 *    - then_block: Executed if condition is true
 *    - else_block: Executed if condition is false
 *    - merge_block: Merges results with PHI node
 * 4. Branch instruction: br i1 %cond, label %then, label %else
 * 5. PHI node selects result based on which block executed
 *
 * Example LLVM IR:
 * ```llvm
 * %cond_val = ...              ; Compute condition
 * %cond = icmp ne i64 %cond_val, 0
 * br i1 %cond, label %then, label %else
 *
 * then:
 *   %then_val = ...            ; Compute then branch
 *   br label %merge
 *
 * else:
 *   %else_val = ...            ; Compute else branch
 *   br label %merge
 *
 * merge:
 *   %result = phi i64 [ %then_val, %then ], [ %else_val, %else ]
 * ```
 */

// Forward declarations for node compilation
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);
extern LLVMTypeRef LLVMCodeGen_getIntType(LLVMCodeGen *gen);
extern LLVMTypeRef LLVMCodeGen_getFloatType(LLVMCodeGen *gen);

/**
 * Compile a block (OP_FUNCTION) as a sequence of statements
 * Used for multi-statement blocks in if/loop/etc.
 *
 * Syntax: {stmt1 stmt2 <- result}
 *
 * Strategy:
 * 1. Compile each child node sequentially (assignments, expressions, etc.)
 * 2. Find the return statement (<-) or use last expression
 * 3. Return the final value
 *
 * Note: This does NOT create a new function - just executes statements inline
 *
 * Public API: Used by if, when, unless, and other control flow constructs
 */
LLVMValueRef LLVMCodeGen_compileBlockAsStatements(LLVMCodeGen *gen, AstNode *blockNode) {
  if (gen->debugMode) {
    fprintf(stderr, "[BLOCK DEBUG] compileBlockAsStatements called, opcode=%d, childCount=%d\n",
            blockNode->opcode, blockNode->childCount);
  }

  if (blockNode->opcode != OP_FUNCTION) {
    // Not a block, compile as regular node
    if (gen->debugMode) {
      fprintf(stderr, "[BLOCK DEBUG] Not OP_FUNCTION, compiling as regular node\n");
    }
    return LLVMCodeGen_compileNode(gen, blockNode);
  }

  // This is a block {stmt1 stmt2 <- result} - compile as statements, not as a function definition

  // Block has no children - return void (0)
  if (blockNode->childCount == 0) {
    if (gen->debugMode) {
      fprintf(stderr, "[BLOCK DEBUG] Empty block, returning 0\n");
    }
    return LLVMConstInt(gen->intType, 0, 0);
  }

  if (gen->debugMode) {
    fprintf(stderr, "[BLOCK DEBUG] Block has %d children, compiling statements\n", blockNode->childCount);
  }

  // Compile each statement in the block
  LLVMValueRef lastValue = NULL;

  for (int i = 0; i < blockNode->childCount; i++) {
    AstNode *child = blockNode->children[i];

    // Special handling for return statements (<- value)
    // In blocks (if/else branches, NOT actual functions), we don't emit a 'ret' instruction
    // Instead, just compile the value and use it as the block's result

    if (gen->debugMode) {
      fprintf(stderr, "[BLOCK DEBUG] Child %d: opcode=%d\n", i, child->opcode);
    }

    // Check if this is a statement containing a return
    if (child->opcode == OP_STATEMENT && child->childCount > 0 &&
        child->children[0]->opcode == OP_RETURN) {
      // This is a statement wrapping a return: (<- value)
      AstNode *returnNode = child->children[0];

      if (gen->debugMode) {
        fprintf(stderr, "[BLOCK DEBUG] Found OP_STATEMENT with OP_RETURN, compiling return value\n");
      }

      //  fix: If we're inside a loop, let the return handler run
      // so it can store the value and branch to loop exit
      if (gen->loopExitBlock != NULL) {
        // Inside loop - compile normally to trigger loop early exit logic
        lastValue = LLVMCodeGen_compileNode(gen, returnNode);
        // Note: lastValue may be NULL if returning void/0 (continue loop)
        // In that case, use 0 as the block result
        if (!lastValue) {
          lastValue = LLVMConstInt(gen->intType, 0, 0);
        }
      } else {
        // Not in loop - bypass return handler to avoid emitting 'ret' instruction
        if (returnNode->childCount == 0) {
          // Return with no value, use 0
          lastValue = LLVMConstInt(gen->intType, 0, 0);
        } else {
          // Compile the return value directly (bypass the return handler)
          // This is crucial: we compile the expression but DON'T emit 'ret'
          lastValue = LLVMCodeGen_compileNode(gen, returnNode->children[0]);
          if (!lastValue) {
            fprintf(stderr, "ERROR: Failed to compile return value in block at line %d\n", returnNode->lineNumber);
            return NULL;
          }
        }
      }
      // Stop processing - return exits the block
      break;
    }

    // Direct OP_RETURN (less common, but handle it too)
    if (child->opcode == OP_RETURN) {
      //  fix: If we're inside a loop, let the return handler run
      if (gen->loopExitBlock != NULL) {
        // Inside loop - compile normally to trigger loop early exit logic
        lastValue = LLVMCodeGen_compileNode(gen, child);
        // Note: lastValue may be NULL if returning void/0 (continue loop)
        if (!lastValue) {
          lastValue = LLVMConstInt(gen->intType, 0, 0);
        }
      } else {
        // Not in loop - bypass return handler
        if (child->childCount == 0) {
          lastValue = LLVMConstInt(gen->intType, 0, 0);
        } else {
          lastValue = LLVMCodeGen_compileNode(gen, child->children[0]);
          if (!lastValue) {
            fprintf(stderr, "ERROR: Failed to compile return value in block at line %d\n", child->lineNumber);
            return NULL;
          }
        }
      }
      break;
    }

    // Compile this statement/expression normally
    lastValue = LLVMCodeGen_compileNode(gen, child);

    if (!lastValue) {
      fprintf(stderr, "ERROR: Failed to compile block statement at line %d\n", child->lineNumber);
      return NULL;
    }
  }

  // Return the last compiled value
  if (!lastValue) {
    // Empty block, return 0
    return LLVMConstInt(gen->intType, 0, 0);
  }

  return lastValue;
}

/**
 * Compile if statement with LLVM branch and PHI nodes
 *
 * Syntax: (if condition then-block [else-block])
 *
 * Arguments:
 * - node: AST node with 2 or 3 children
 *   - 2 children: condition, then-block (else returns 0)
 *   - 3 children: condition, then-block, else-block
 *
 * Returns: LLVMValueRef - Result from executed branch
 *
 * Error handling:
 * - Validates 2 or 3 arguments
 * - Checks condition can be evaluated
 * - Ensures both branches return compatible types
 * - Uses PHI node for type-safe result merging
 * - Optional else: returns 0 if condition false
 */
LLVMValueRef LLVMCodeGen_compileIf(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count (condition, then, [else])
  //  Support optional else (2 or 3 arguments)
  if (node->childCount < 2 || node->childCount > 3) {
    fprintf(stderr, "ERROR: if requires 2 or 3 arguments (condition then [else]) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  AstNode *condNode = node->children[0];
  AstNode *thenNode = node->children[1];
  AstNode *elseNode = (node->childCount == 3) ? node->children[2] : NULL;

  // Step 1: Compile condition expression
  LLVMValueRef condValue = LLVMCodeGen_compileNode(gen, condNode);
  if (!condValue) {
    fprintf(stderr, "ERROR: Failed to compile if condition at line %d\n", node->lineNumber);
    return NULL;
  }

  // CRITICAL FIX: Unbox Generic* pointers before checking truthiness
  // CLOSURE calls return Generic* pointers cast to i64 that need unboxing
  // Direct function calls (is, greater_than, etc.) return raw i64 that don't need unboxing
  // We need to check if this is a CLOSURE CALL specifically

  // Check if the condition is a closure call (OP_APPLICATION where function is a closure)
  if (condNode->opcode == OP_APPLICATION && condNode->childCount > 0) {
    AstNode *funcNode = condNode->children[0];

    // Check if the function being called is a closure (OP_IDENTIFIER referring to a closure)
    int isClosure = 0;
    if (funcNode->opcode == OP_IDENTIFIER) {
      // Check if this identifier refers to a closure
      if (gen->closures && LLVMVariableMap_get(gen->closures, funcNode->val)) {
        isClosure = 1;
        fprintf(stderr, "[IF DEBUG] Condition is CLOSURE call '%s', will unbox Generic* result\n", funcNode->val);
      }
    }

    // Only unbox for closure calls
    if (isClosure) {
      // Declare franz_unbox_int if needed
      LLVMValueRef unboxIntFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_int");
      if (!unboxIntFunc) {
        LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
        LLVMTypeRef params[] = { genericPtrType };
        LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
        unboxIntFunc = LLVMAddFunction(gen->module, "franz_unbox_int", funcType);
      }

      // Convert i64 to Generic* pointer
      LLVMValueRef genericPtr = LLVMBuildIntToPtr(gen->builder, condValue,
                                                   LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
                                                   "cond_as_ptr");

      // Call franz_unbox_int to get the actual integer value
      LLVMValueRef args[] = { genericPtr };
      condValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxIntFunc),
                                  unboxIntFunc, args, 1, "unboxed_cond");
      fprintf(stderr, "[IF DEBUG] Unboxed Generic* pointer to get actual value\n");
    }
  }

  // CRITICAL FIX Issue #3: Also check if condition is a VARIABLE containing a closure result
  // Variables assigned from closure calls (e.g., check1 = (is_sorted_three ...)) need unboxing
  if (condNode->opcode == OP_IDENTIFIER && condNode->val) {
    // Check typeMetadata to see if this variable was assigned from a function call
    void *metadata = LLVMVariableMap_get(gen->typeMetadata, condNode->val);
    if (metadata) {
      int storedOpcode = (int)(intptr_t)metadata - 1;  // Subtract 1 (was added to avoid NULL)

      // If variable was assigned from OP_APPLICATION (function call), it's boxed as Generic*
      if (storedOpcode == OP_APPLICATION) {
        // Declare franz_unbox_int if needed
        LLVMValueRef unboxIntFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_int");
        if (!unboxIntFunc) {
          LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
          LLVMTypeRef params[] = { genericPtrType };
          LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
          unboxIntFunc = LLVMAddFunction(gen->module, "franz_unbox_int", funcType);
        }

        // Convert i64 to Generic* pointer
        LLVMValueRef genericPtr = LLVMBuildIntToPtr(gen->builder, condValue,
                                                     LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
                                                     "var_cond_as_ptr");

        // Call franz_unbox_int to get the actual integer value
        LLVMValueRef args[] = { genericPtr };
        condValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxIntFunc),
                                    unboxIntFunc, args, 1, "var_unboxed_cond");
      }
    }
  }

  // Step 2: Convert condition to i1 boolean
  // Any non-zero value is true, zero is false
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef condition = LLVMBuildICmp(gen->builder, LLVMIntNE, condValue, zero, "if_cond");

  // Step 3: Create basic blocks for control flow
  LLVMBasicBlockRef thenBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "then");
  LLVMBasicBlockRef elseBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "else");
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(
      gen->context, gen->currentFunction, "merge");

  // Step 4: Branch based on condition
  LLVMBuildCondBr(gen->builder, condition, thenBlock, elseBlock);

  // Step 5: Compile then branch
  // Support both direct values and multi-statement blocks
  LLVMPositionBuilderAtEnd(gen->builder, thenBlock);
  LLVMValueRef thenValue = LLVMCodeGen_compileBlockAsStatements(gen, thenNode);
  
  //  fix: Check if block has a terminator (e.g., loop early exit via return)
  // If so, thenValue might be NULL (void) but that's OK - the block already branched elsewhere
  if (!thenValue && !LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(gen->builder))) {
    fprintf(stderr, "ERROR: Failed to compile then branch at line %d\n", thenNode->lineNumber);
    return NULL;
  }
  
  // If thenValue is NULL but block has terminator, use 0 as placeholder
  if (!thenValue) {
    thenValue = LLVMConstInt(gen->intType, 0, 0);
  }

  // Get the block where then branch ended (might have changed due to nested control flow)
  LLVMBasicBlockRef thenEndBlock = LLVMGetInsertBlock(gen->builder);

  //  Track whether then branch reaches merge
  // Check BEFORE adding branch instruction
  int thenReachesMerge = !LLVMGetBasicBlockTerminator(thenEndBlock);

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Then branch reaches merge: %d\n", thenReachesMerge);
  }

  // Branch to merge (only if block doesn't already have terminator)
  if (thenReachesMerge) {
    LLVMBuildBr(gen->builder, mergeBlock);
  }

  // Step 6: Compile else branch (or use default 0 if optional else)
  //  Support optional else
  LLVMPositionBuilderAtEnd(gen->builder, elseBlock);
  LLVMValueRef elseValue;

  if (elseNode == NULL) {
    // Optional else: return 0 (i64)
    elseValue = LLVMConstInt(gen->intType, 0, 0);
  } else {
    // Explicit else: compile as normal
    elseValue = LLVMCodeGen_compileBlockAsStatements(gen, elseNode);

    //  fix: Check if block has a terminator (e.g., loop early exit via return)
    if (!elseValue && !LLVMGetBasicBlockTerminator(LLVMGetInsertBlock(gen->builder))) {
      fprintf(stderr, "ERROR: Failed to compile else branch at line %d\n", elseNode->lineNumber);
      return NULL;
    }

    // If elseValue is NULL but block has terminator, use 0 as placeholder
    if (!elseValue) {
      elseValue = LLVMConstInt(gen->intType, 0, 0);
    }
  }

  // Get the block where else branch ended
  LLVMBasicBlockRef elseEndBlock = LLVMGetInsertBlock(gen->builder);

  //  Track whether else branch reaches merge
  // Check BEFORE adding branch instruction
  int elseReachesMerge = !LLVMGetBasicBlockTerminator(elseEndBlock);

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Else branch reaches merge: %d\n", elseReachesMerge);
  }

  // Branch to merge (only if block doesn't already have terminator)
  if (elseReachesMerge) {
    LLVMBuildBr(gen->builder, mergeBlock);
  }

  // Step 7: Create PHI node in merge block
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);

  // Get result types
  LLVMTypeRef thenType = LLVMTypeOf(thenValue);
  LLVMTypeRef elseType = LLVMTypeOf(elseValue);

  // Handle type conversion if necessary
  if (thenType != elseType) {
    // Check if one branch returns void (int 0) and the other returns a value
    // This is a common pattern in Franz for loop continuations
    int thenIsVoid = (thenType == gen->intType && LLVMIsConstant(thenValue) && 
                      LLVMConstIntGetZExtValue(thenValue) == 0);
    int elseIsVoid = (elseType == gen->intType && LLVMIsConstant(elseValue) && 
                      LLVMConstIntGetZExtValue(elseValue) == 0);
    
    if (thenIsVoid && !elseIsVoid) {
      // Then returns void, else returns value - use else type
      // Convert then to match else type (create a zero/null value of else type)
      // Position builder in then block before terminator
      LLVMValueRef thenTerm = LLVMGetBasicBlockTerminator(thenEndBlock);
      if (thenTerm) {
        LLVMPositionBuilderBefore(gen->builder, thenTerm);
      } else {
        LLVMPositionBuilderAtEnd(gen->builder, thenEndBlock);
      }

      if (elseType == gen->intType) {
        thenValue = LLVMConstInt(gen->intType, 0, 0);
      } else if (elseType == gen->floatType) {
        thenValue = LLVMConstReal(gen->floatType, 0.0);
      } else if (LLVMGetTypeKind(elseType) == LLVMPointerTypeKind) {
        // For pointer types (strings), use empty string
        thenValue = LLVMBuildGlobalStringPtr(gen->builder, "", ".empty_void_then");
      } else {
        // Fallback: use a typed null to match the else branch type (e.g., i32 printf return)
        thenValue = LLVMConstNull(elseType);
      }
      thenType = elseType;
      LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
    } else if (elseIsVoid && !thenIsVoid) {
      // Else returns void, then returns value - use then type
      // Position builder in else block before terminator
      LLVMValueRef elseTerm = LLVMGetBasicBlockTerminator(elseEndBlock);
      if (elseTerm) {
        LLVMPositionBuilderBefore(gen->builder, elseTerm);
      } else {
        LLVMPositionBuilderAtEnd(gen->builder, elseEndBlock);
      }

      if (thenType == gen->intType) {
        elseValue = LLVMConstInt(gen->intType, 0, 0);
      } else if (thenType == gen->floatType) {
        elseValue = LLVMConstReal(gen->floatType, 0.0);
      } else if (LLVMGetTypeKind(thenType) == LLVMPointerTypeKind) {
        // For pointer types (strings), use empty string
        elseValue = LLVMBuildGlobalStringPtr(gen->builder, "", ".empty_void_else");
      } else {
        // Fallback: use a typed null to match the then branch type (e.g., i32 printf return)
        elseValue = LLVMConstNull(thenType);
      }
      elseType = thenType;
      LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
    } else if (thenType == gen->intType && elseType == gen->floatType) {
      // If one is int and other is float, promote int to float
      LLVMPositionBuilderAtEnd(gen->builder, thenEndBlock);
      // Find insertion point before terminator
      LLVMValueRef terminator = LLVMGetBasicBlockTerminator(thenEndBlock);
      if (terminator) {
        LLVMPositionBuilderBefore(gen->builder, terminator);
      }
      thenValue = LLVMBuildSIToFP(gen->builder, thenValue, gen->floatType, "then_itof");
      thenType = gen->floatType;
      LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
    } else if (thenType == gen->floatType && elseType == gen->intType) {
      LLVMPositionBuilderAtEnd(gen->builder, elseEndBlock);
      // Find insertion point before terminator
      LLVMValueRef terminator = LLVMGetBasicBlockTerminator(elseEndBlock);
      if (terminator) {
        LLVMPositionBuilderBefore(gen->builder, terminator);
      }
      elseValue = LLVMBuildSIToFP(gen->builder, elseValue, gen->floatType, "else_itof");
      elseType = gen->floatType;
      LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
    } else if ((LLVMGetTypeKind(thenType) == LLVMPointerTypeKind && LLVMGetTypeKind(elseType) == LLVMIntegerTypeKind) ||
               (LLVMGetTypeKind(thenType) == LLVMIntegerTypeKind && LLVMGetTypeKind(elseType) == LLVMPointerTypeKind) ||
               (LLVMGetTypeKind(thenType) == LLVMIntegerTypeKind && LLVMGetTypeKind(elseType) == LLVMIntegerTypeKind &&
                LLVMGetIntTypeWidth(thenType) != LLVMGetIntTypeWidth(elseType))) {
      // Fallback: normalize mixed pointer/int (or differing int widths) to i64
      if (LLVMGetTypeKind(thenType) == LLVMPointerTypeKind) {
        LLVMPositionBuilderAtEnd(gen->builder, thenEndBlock);
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(thenEndBlock);
        if (terminator) {
          LLVMPositionBuilderBefore(gen->builder, terminator);
        }
        thenValue = LLVMBuildPtrToInt(gen->builder, thenValue, gen->intType, "then_ptoi");
      } else if (LLVMGetTypeKind(thenType) == LLVMIntegerTypeKind &&
                 LLVMGetIntTypeWidth(thenType) != LLVMGetIntTypeWidth(gen->intType)) {
        LLVMPositionBuilderAtEnd(gen->builder, thenEndBlock);
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(thenEndBlock);
        if (terminator) {
          LLVMPositionBuilderBefore(gen->builder, terminator);
        }
        thenValue = LLVMBuildSExt(gen->builder, thenValue, gen->intType, "then_sext");
      }

      if (LLVMGetTypeKind(elseType) == LLVMPointerTypeKind) {
        LLVMPositionBuilderAtEnd(gen->builder, elseEndBlock);
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(elseEndBlock);
        if (terminator) {
          LLVMPositionBuilderBefore(gen->builder, terminator);
        }
        elseValue = LLVMBuildPtrToInt(gen->builder, elseValue, gen->intType, "else_ptoi");
      } else if (LLVMGetTypeKind(elseType) == LLVMIntegerTypeKind &&
                 LLVMGetIntTypeWidth(elseType) != LLVMGetIntTypeWidth(gen->intType)) {
        LLVMPositionBuilderAtEnd(gen->builder, elseEndBlock);
        LLVMValueRef terminator = LLVMGetBasicBlockTerminator(elseEndBlock);
        if (terminator) {
          LLVMPositionBuilderBefore(gen->builder, terminator);
        }
        elseValue = LLVMBuildSExt(gen->builder, elseValue, gen->intType, "else_sext");
      }

      thenType = elseType = gen->intType;
      LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
    } else if (thenType != elseType) {
      // Types are incompatible
      fprintf(stderr, "ERROR: if branches return incompatible types at line %d\n", node->lineNumber);
      return NULL;
    }
  }

  //  +  Check which branches actually reach the merge block
  // (Already tracked above - thenReachesMerge and elseReachesMerge set before adding branches)

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Creating PHI node - then reaches: %d, else reaches: %d\n",
            thenReachesMerge, elseReachesMerge);
  }

  if (!thenReachesMerge && !elseReachesMerge) {
    // Neither branch reaches merge - both have early exit
    // The merge block is unreachable, just return a dummy value
    if (gen->debugMode) {
      fprintf(stderr, "[DEBUG] Neither branch reaches merge, returning 0\n");
    }
    return LLVMConstInt(gen->intType, 0, 0);
  } else if (!thenReachesMerge) {
    // Only else reaches merge - return else value directly
    if (gen->debugMode) {
      fprintf(stderr, "[DEBUG] Only else reaches merge\n");
    }
    return elseValue;
  } else if (!elseReachesMerge) {
    // Only then reaches merge - return then value directly
    if (gen->debugMode) {
      fprintf(stderr, "[DEBUG] Only then reaches merge\n");
    }
    return thenValue;
  } else {
    // Both branches reach merge - create PHI node
    if (gen->debugMode) {
      fprintf(stderr, "[DEBUG] Both branches reach merge, creating PHI node\n");
    }
    LLVMValueRef phi = LLVMBuildPhi(gen->builder, thenType, "if_result");

    LLVMValueRef incomingValues[] = {thenValue, elseValue};
    LLVMBasicBlockRef incomingBlocks[] = {thenEndBlock, elseEndBlock};
    LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);

    return phi;
  }
}
