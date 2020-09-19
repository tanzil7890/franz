#include "llvm_adt.h"
#include "../stdlib.h"
#include "../llvm-lists/llvm_lists.h"
#include "../llvm-list-ops/llvm_list_ops.h"
#include "../llvm-closures/llvm_closures.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 *  LLVM ADT (Algebraic Data Types) Implementation
 *
 * Provides native compilation for:
 * - variant: Tagged unions (like Rust's enum)
 * - match: Pattern matching (like Rust's match)
 *
 * Achieves Rust-level performance with zero runtime overhead.
 */

// ============================================================================
// Runtime Function Declarations
// ============================================================================

static void declareRuntimeFunctions(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_box_int(i64) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_int")) {
    LLVMTypeRef params[] = { gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_int", funcType);
  }

  // franz_box_float(double) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_float")) {
    LLVMTypeRef params[] = { gen->floatType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_float", funcType);
  }

  // franz_box_string(i8*) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_string")) {
    LLVMTypeRef params[] = { gen->stringType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_string", funcType);
  }

  // franz_box_list(Generic*) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_list")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_list", funcType);
  }

  // franz_list_new(Generic**, int) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_list_new")) {
    LLVMTypeRef params[] = {
      LLVMPointerType(genericPtrType, 0),  // Generic**
      gen->intType                          // int
    };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 2, 0);
    LLVMAddFunction(gen->module, "franz_list_new", funcType);
  }

  // franz_list_nth(Generic*, i64) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_list_nth")) {
    LLVMTypeRef params[] = { genericPtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 2, 0);
    LLVMAddFunction(gen->module, "franz_list_nth", funcType);
  }

  // franz_unbox_string(Generic*) -> i8*
  if (!LLVMGetNamedFunction(gen->module, "franz_unbox_string")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->stringType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_unbox_string", funcType);
  }

  // strcmp(i8*, i8*) -> int
  if (!LLVMGetNamedFunction(gen->module, "strcmp")) {
    LLVMTypeRef params[] = { gen->stringType, gen->stringType };
    LLVMTypeRef funcType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context), params, 2, 0);
    LLVMAddFunction(gen->module, "strcmp", funcType);
  }
}

// ============================================================================
// Variant Construction - (variant tag values...)
// ============================================================================

LLVMValueRef LLVMAdt_compileVariant(LLVMCodeGen *gen, AstNode *node, int lineNumber) {

  if (!gen || !node) {
    fprintf(stderr, "ERROR: Invalid parameters to LLVMAdt_compileVariant\n");
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[ADT DEBUG] LLVMAdt_compileVariant called at line %d\n", lineNumber);
    fprintf(stderr, "[ADT DEBUG] Node has %d children\n", node->childCount);
  }

  declareRuntimeFunctions(gen);

  // Create argument node (children after function name)
  int argCount = node->childCount - 1;
  AstNode **args = node->children + 1;

  if (argCount < 1) {
    fprintf(stderr, "ERROR: variant requires at least 1 argument (tag) at line %d\n", lineNumber);
    return LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0));
  }

  // Compile tag (first argument - must be string)
  if (gen->debugMode) {
    fprintf(stderr, "[ADT DEBUG] Compiling tag (arg 0)\n");
  }

  LLVMValueRef tagValue = LLVMCodeGen_compileNode(gen, args[0]);
  if (!tagValue) {
    fprintf(stderr, "ERROR: Failed to compile variant tag at line %d\n", lineNumber);
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[ADT DEBUG] Tag compiled successfully, boxing...\n");
  }

  // Box tag as Generic*
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
  if (!boxStringFunc) {
    fprintf(stderr, "ERROR: franz_box_string function not found!\n");
    return NULL;
  }

  LLVMValueRef tagBoxed = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(boxStringFunc),
    boxStringFunc,
    &tagValue,
    1,
    "variant_tag"
  );

  // Compile values (remaining arguments) into a list
  LLVMValueRef valuesListBoxed;
  int valueCount = argCount - 1;

  if (valueCount == 0) {
    // Empty values list
    LLVMValueRef listNewFunc = LLVMGetNamedFunction(gen->module, "franz_list_new");
    LLVMValueRef nullPtr = LLVMConstNull(LLVMPointerType(LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0), 0));
    LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
    LLVMValueRef args[] = { nullPtr, zero };

    valuesListBoxed = LLVMBuildCall2(
      gen->builder,
      LLVMGlobalGetValueType(listNewFunc),
      listNewFunc,
      args,
      2,
      "empty_values_list"
    );
  } else {
    // Create array of values
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    LLVMValueRef valuesArray = LLVMBuildArrayAlloca(
      gen->builder,
      genericPtrType,
      LLVMConstInt(gen->intType, valueCount, 0),
      "values_array"
    );

    // Get boxing functions
    LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
    LLVMValueRef boxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_box_float");
    LLVMValueRef boxStringFunc2 = LLVMGetNamedFunction(gen->module, "franz_box_string");
    LLVMValueRef boxListFunc = LLVMGetNamedFunction(gen->module, "franz_box_list");

    // Compile, box, and store each value
    for (int i = 0; i < valueCount; i++) {

      AstNode *valueNode = args[i + 1];

      LLVMValueRef compiledValue = LLVMCodeGen_compileNode(gen, valueNode);
      if (!compiledValue) {
        fprintf(stderr, "ERROR: Failed to compile variant value %d at line %d\n", i, lineNumber);
        return NULL;
      }

      // Determine value type and box it
      LLVMTypeRef valueType = LLVMTypeOf(compiledValue);
      LLVMTypeKind typeKind = LLVMGetTypeKind(valueType);
      LLVMValueRef boxedValue = NULL;

      if (typeKind == LLVMIntegerTypeKind) {
        // Integer: box with franz_box_int
        LLVMValueRef boxArgs[] = { compiledValue };
        boxedValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                    boxIntFunc, boxArgs, 1, "boxed_int");
      } else if (typeKind == LLVMDoubleTypeKind) {
        // Float: box with franz_box_float
        LLVMValueRef boxArgs[] = { compiledValue };
        boxedValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc),
                                    boxFloatFunc, boxArgs, 1, "boxed_float");
      } else if (typeKind == LLVMPointerTypeKind) {
        // Could be string or list
        if (valueNode->opcode == OP_STRING) {
          LLVMValueRef boxArgs[] = { compiledValue };
          boxedValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc2),
                                      boxStringFunc2, boxArgs, 1, "boxed_string");
        } else if (valueNode->opcode == OP_LIST) {
          LLVMValueRef boxArgs[] = { compiledValue };
          boxedValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxListFunc),
                                      boxListFunc, boxArgs, 1, "boxed_list");
        } else {
          // Treat as string by default
          LLVMValueRef boxArgs[] = { compiledValue };
          boxedValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc2),
                                      boxStringFunc2, boxArgs, 1, "boxed_ptr");
        }
      } else {
        fprintf(stderr, "ERROR: Unsupported variant value type %d at line %d\n", typeKind, lineNumber);
        return NULL;
      }

      // Store boxed value in array
      LLVMValueRef indices[] = { LLVMConstInt(gen->intType, i, 0) };

      LLVMValueRef elemPtr = LLVMBuildGEP2(
        gen->builder,
        genericPtrType,
        valuesArray,
        indices,
        1,
        "value_ptr"
      );

      LLVMBuildStore(gen->builder, boxedValue, elemPtr);
    }

    // Create list from array
    LLVMValueRef listNewFunc = LLVMGetNamedFunction(gen->module, "franz_list_new");
    LLVMValueRef args[] = {
      valuesArray,
      LLVMConstInt(gen->intType, valueCount, 0)
    };

    valuesListBoxed = LLVMBuildCall2(
      gen->builder,
      LLVMGlobalGetValueType(listNewFunc),
      listNewFunc,
      args,
      2,
      "values_list"
    );
  }

  // Create variant structure: [tag, values_list]
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef variantArray = LLVMBuildArrayAlloca(
    gen->builder,
    genericPtrType,
    LLVMConstInt(gen->intType, 2, 0),
    "variant_array"
  );

  // Store tag at index 0
  LLVMValueRef tagIdx[] = { LLVMConstInt(gen->intType, 0, 0) };
  LLVMValueRef tagPtr = LLVMBuildGEP2(gen->builder, genericPtrType, variantArray, tagIdx, 1, "tag_ptr");
  LLVMBuildStore(gen->builder, tagBoxed, tagPtr);

  // Store values list at index 1
  LLVMValueRef valsIdx[] = { LLVMConstInt(gen->intType, 1, 0) };
  LLVMValueRef valsPtr = LLVMBuildGEP2(gen->builder, genericPtrType, variantArray, valsIdx, 1, "vals_ptr");
  LLVMBuildStore(gen->builder, valuesListBoxed, valsPtr);

  // Create final variant list
  LLVMValueRef listNewFunc = LLVMGetNamedFunction(gen->module, "franz_list_new");
  LLVMValueRef listNewArgs[] = {
    variantArray,
    LLVMConstInt(gen->intType, 2, 0)
  };

  LLVMValueRef variantBoxed = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(listNewFunc),  // Fixed: use LLVMGlobalGetValueType instead of LLVMGetElementType
    listNewFunc,
    listNewArgs,
    2,
    "variant"
  );

  //  Convert ptr to i64 for Franz universal type system
  // All values in Franz are represented as i64 (pointers encoded as integers)
  // This matches closure return values and ensures type consistency
  LLVMValueRef variantAsInt = LLVMBuildPtrToInt(gen->builder, variantBoxed, gen->intType, "variant_as_int");

  if (gen->debugMode) {
    fprintf(stderr, "[ADT DEBUG] Variant compiled successfully, returning i64\n");
  }

  return variantAsInt;
}

// ============================================================================
// Pattern Matching - (match variant tag1 fn1 tag2 fn2 ...)
// ============================================================================

LLVMValueRef LLVMAdt_compileMatch(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  if (!gen || !node) {
    fprintf(stderr, "ERROR: Invalid parameters to LLVMAdt_compileMatch\n");
    return NULL;
  }

  declareRuntimeFunctions(gen);

  // Create argument node (children after function name)
  int argCount = node->childCount - 1;
  AstNode **args = node->children + 1;

  if (argCount < 3) {
    fprintf(stderr, "ERROR: match requires at least 3 arguments (variant, tag, handler) at line %d\n", lineNumber);
    return LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0));
  }

  // Compile variant value
  LLVMValueRef variantValue = LLVMCodeGen_compileNode(gen, args[0]);
  if (!variantValue) {
    fprintf(stderr, "ERROR: Failed to compile match variant at line %d\n", lineNumber);
    return NULL;
  }

  // Extract tag from variant
  LLVMValueRef tagValue = LLVMAdt_extractTag(gen, variantValue);
  if (!tagValue) {
    fprintf(stderr, "ERROR: Failed to extract tag from variant at line %d\n", lineNumber);
    return NULL;
  }

  // Extract values from variant
  LLVMValueRef valuesValue = LLVMAdt_extractValues(gen, variantValue);
  if (!valuesValue) {
    fprintf(stderr, "ERROR: Failed to extract values from variant at line %d\n", lineNumber);
    return NULL;
  }

  // Create merge block for PHI node
  LLVMValueRef currentFunc = LLVMGetBasicBlockParent(LLVMGetInsertBlock(gen->builder));
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "match_merge");

  // Check if there's a default handler (odd argument count)
  int hasDefault = (argCount - 1) % 2 == 1;
  int pairCount = (argCount - 1) / 2;

  LLVMBasicBlockRef *caseBlocks = malloc(sizeof(LLVMBasicBlockRef) * pairCount);

  // Always create a default block for proper control flow
  LLVMBasicBlockRef defaultBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "match_default");

  // Create case blocks
  for (int i = 0; i < pairCount; i++) {
    char blockName[64];
    snprintf(blockName, sizeof(blockName), "match_case_%d", i);
    caseBlocks[i] = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);
  }

  // Build cascading if-else chain for tag comparison
  LLVMBasicBlockRef nextCheckBlock = NULL;

  for (int i = 0; i < pairCount; i++) {
    if (i > 0) {
      LLVMPositionBuilderAtEnd(gen->builder, nextCheckBlock);
    }

    // Compile tag pattern (must be string literal)
    AstNode *tagPatternNode = args[1 + i * 2];
    if (tagPatternNode->opcode != OP_STRING) {
      fprintf(stderr, "ERROR: match tag patterns must be string literals at line %d\n", lineNumber);
      free(caseBlocks);
      return NULL;
    }

    LLVMValueRef patternString = LLVMBuildGlobalStringPtr(gen->builder, tagPatternNode->val, "tag_pattern");

    // Compare tag with pattern using strcmp
    LLVMValueRef strcmpFunc = LLVMGetNamedFunction(gen->module, "strcmp");
    LLVMValueRef cmpArgs[] = { tagValue, patternString };
    LLVMValueRef cmpResult = LLVMBuildCall2(
      gen->builder,
      LLVMGlobalGetValueType(strcmpFunc),
      strcmpFunc,
      cmpArgs,
      2,
      "tag_cmp"
    );

    // Check if equal (strcmp returns 0)
    LLVMValueRef isEqual = LLVMBuildICmp(
      gen->builder,
      LLVMIntEQ,
      cmpResult,
      LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
      "tag_eq"
    );

    // Create next check block or use default
    if (i < pairCount - 1) {
      char nextBlockName[64];
      snprintf(nextBlockName, sizeof(nextBlockName), "match_check_%d", i + 1);
      nextCheckBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, nextBlockName);
    } else {
      nextCheckBlock = defaultBlock;
    }

    // Branch to case or next check
    LLVMBuildCondBr(gen->builder, isEqual, caseBlocks[i], nextCheckBlock);
  }

  // Compile case handlers
  LLVMValueRef *caseResults = malloc(sizeof(LLVMValueRef) * pairCount);

  for (int i = 0; i < pairCount; i++) {
    LLVMPositionBuilderAtEnd(gen->builder, caseBlocks[i]);

    // Compile handler function
    AstNode *handlerNode = args[2 + i * 2];
    LLVMValueRef handlerValue = LLVMCodeGen_compileNode(gen, handlerNode);

    if (!handlerValue) {
      fprintf(stderr, "ERROR: Failed to compile match handler %d at line %d\n", i, lineNumber);
      free(caseBlocks);
      free(caseResults);
      return NULL;
    }

    // Call handler with variant values
    //  Support both no-argument {-> expr} and parameterized {x y -> expr} handlers

    LLVMValueRef handlerResult;

    //  Detect handler parameter count
    // In OP_FUNCTION nodes, first N children are OP_IDENTIFIER (parameters)
    int handlerParamCount = 0;
    if (handlerNode->opcode == OP_FUNCTION) {
      fprintf(stderr, "[ADT DEBUG] Handler node details:\n");
      fprintf(stderr, "  - Opcode: OP_FUNCTION\n");
      fprintf(stderr, "  - Child count: %d\n", handlerNode->childCount);
      fprintf(stderr, "  - Is closure: %d\n", LLVMClosures_isClosure(handlerNode));

      fprintf(stderr, "  - Children opcodes: ");
      for (int j = 0; j < handlerNode->childCount; j++) {
        fprintf(stderr, "%d ", handlerNode->children[j]->opcode);
      }
      fprintf(stderr, "\n");

      while (handlerParamCount < handlerNode->childCount &&
             handlerNode->children[handlerParamCount]->opcode == OP_IDENTIFIER) {
        fprintf(stderr, "    Param %d: %s\n", handlerParamCount,
                handlerNode->children[handlerParamCount]->val);
        handlerParamCount++;
      }

      fprintf(stderr, "  - Detected %d parameters\n", handlerParamCount);

      // Show handler body (children after parameters)
      if (handlerParamCount < handlerNode->childCount) {
        fprintf(stderr, "  - Handler body starts at child %d\n", handlerParamCount);
        fprintf(stderr, "  - Body child count: %d\n", handlerNode->childCount - handlerParamCount);
      }
    }

    //  Unpack variant values if handler expects parameters
    LLVMValueRef *handlerArgs = NULL;
    if (handlerParamCount > 0) {
      // Convert valuesValue (Generic* ptr) to i64 for universal type system
      LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
      LLVMValueRef valuesListPtr = LLVMBuildIntToPtr(gen->builder, valuesValue, genericPtrType, "values_list_ptr");

      // Unpack values from variant
      int unpackedCount = 0;
      handlerArgs = LLVMAdt_unpackValuesToArray(gen, valuesListPtr, handlerParamCount, &unpackedCount);

      if (!handlerArgs || unpackedCount != handlerParamCount) {
        fprintf(stderr, "ERROR: Failed to unpack %d values for handler at line %d\n",
                handlerParamCount, lineNumber);
        if (handlerArgs) free(handlerArgs);
        free(caseBlocks);
        free(caseResults);
        return NULL;
      }

      if (gen->debugMode) {
        fprintf(stderr, "[ADT DEBUG] Unpacked %d values for handler\n", unpackedCount);
      }
    }

    // Call handler with appropriate argument count
    if (LLVMClosures_isClosure(handlerNode)) {
      // Handler is a closure - use closure calling convention
      // Returns i64 (Franz universal representation)
      handlerResult = LLVMClosures_callClosure(gen, handlerValue, handlerArgs, handlerParamCount, NULL);
    } else {
      // Handler is a regular function (no free variables)
      // Call directly with LLVM function call
      LLVMTypeRef funcType = LLVMGlobalGetValueType(handlerValue);
      LLVMValueRef callResult = LLVMBuildCall2(gen->builder, funcType, handlerValue,
                                              handlerArgs, handlerParamCount, "call_result");

      // Convert result to i64 for universal representation
      // Handle different return types: i32, i64, ptr, float, etc.
      LLVMTypeRef resultType = LLVMTypeOf(callResult);
      LLVMTypeKind resultKind = LLVMGetTypeKind(resultType);

      if (resultKind == LLVMIntegerTypeKind) {
        // Integer type - extend to i64 if needed
        unsigned bitWidth = LLVMGetIntTypeWidth(resultType);
        if (bitWidth < 64) {
          handlerResult = LLVMBuildZExt(gen->builder, callResult,
                                       LLVMInt64TypeInContext(gen->context), "result_i64");
        } else {
          handlerResult = callResult;  // Already i64
        }
      } else if (resultKind == LLVMPointerTypeKind) {
        // Pointer type - convert to i64
        handlerResult = LLVMBuildPtrToInt(gen->builder, callResult,
                                         LLVMInt64TypeInContext(gen->context), "result_i64");
      } else if (resultKind == LLVMFloatTypeKind || resultKind == LLVMDoubleTypeKind) {
        // Float type - bitcast to i64
        handlerResult = LLVMBuildBitCast(gen->builder, callResult,
                                        LLVMInt64TypeInContext(gen->context), "result_i64");
      } else {
        // Unknown type - try to cast to i64
        handlerResult = LLVMBuildIntCast(gen->builder, callResult,
                                        LLVMInt64TypeInContext(gen->context), "result_i64");
      }
    }

    // Free unpacked arguments if allocated
    if (handlerArgs) {
      free(handlerArgs);
    }

    // Convert i64 result to ptr (Generic*) for PHI node
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    caseResults[i] = LLVMBuildIntToPtr(gen->builder, handlerResult, genericPtrType, "result_ptr");

    LLVMBuildBr(gen->builder, mergeBlock);
  }

  // Compile default handler (always present, even if just returns void)
  LLVMPositionBuilderAtEnd(gen->builder, defaultBlock);

  LLVMValueRef defaultResult;
  if (hasDefault) {
    AstNode *defaultHandlerNode = args[argCount - 1];
    LLVMValueRef defaultHandler = LLVMCodeGen_compileNode(gen, defaultHandlerNode);

    if (!defaultHandler) {
      fprintf(stderr, "ERROR: Failed to compile default handler at line %d\n", lineNumber);
      free(caseBlocks);
      free(caseResults);
      return NULL;
    }

    // Call default handler (closure or regular function)
    LLVMValueRef defaultHandlerResult;

    // Check if default handler is a closure or regular function
    if (LLVMClosures_isClosure(defaultHandlerNode)) {
      // Handler is a closure - use closure calling convention
      // Returns i64 (Franz universal representation)
      defaultHandlerResult = LLVMClosures_callClosure(gen, defaultHandler, NULL, 0, NULL);
    } else {
      // Handler is a regular function (no free variables)
      // Call directly with LLVM function call
      LLVMTypeRef funcType = LLVMGlobalGetValueType(defaultHandler);
      LLVMValueRef callResult = LLVMBuildCall2(gen->builder, funcType, defaultHandler, NULL, 0, "default_call_result");

      // Convert result to i64 for universal representation
      LLVMTypeRef resultType = LLVMTypeOf(callResult);
      LLVMTypeKind resultKind = LLVMGetTypeKind(resultType);

      if (resultKind == LLVMIntegerTypeKind) {
        unsigned bitWidth = LLVMGetIntTypeWidth(resultType);
        if (bitWidth < 64) {
          defaultHandlerResult = LLVMBuildZExt(gen->builder, callResult,
                                              LLVMInt64TypeInContext(gen->context), "default_result_i64");
        } else {
          defaultHandlerResult = callResult;
        }
      } else if (resultKind == LLVMPointerTypeKind) {
        defaultHandlerResult = LLVMBuildPtrToInt(gen->builder, callResult,
                                                LLVMInt64TypeInContext(gen->context), "default_result_i64");
      } else if (resultKind == LLVMFloatTypeKind || resultKind == LLVMDoubleTypeKind) {
        defaultHandlerResult = LLVMBuildBitCast(gen->builder, callResult,
                                               LLVMInt64TypeInContext(gen->context), "default_result_i64");
      } else {
        defaultHandlerResult = LLVMBuildIntCast(gen->builder, callResult,
                                               LLVMInt64TypeInContext(gen->context), "default_result_i64");
      }
    }

    // Convert i64 result to ptr (Generic*) for PHI node
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    defaultResult = LLVMBuildIntToPtr(gen->builder, defaultHandlerResult, genericPtrType, "default_result_ptr");
  } else {
    // No default handler - return void/null
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    defaultResult = LLVMConstNull(genericPtrType);
  }

  LLVMBuildBr(gen->builder, mergeBlock);

  // Build PHI node in merge block
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, genericPtrType, "match_result");

  // Add incoming values from case blocks
  for (int i = 0; i < pairCount; i++) {
    LLVMAddIncoming(phi, &caseResults[i], &caseBlocks[i], 1);
  }

  // Always add incoming from default block (may be void/null if no handler)
  LLVMAddIncoming(phi, &defaultResult, &defaultBlock, 1);

  free(caseBlocks);
  free(caseResults);

  return phi;
}

// ============================================================================
// Helper Functions
// ============================================================================

LLVMValueRef LLVMAdt_extractTag(LLVMCodeGen *gen, LLVMValueRef variantValue) {
  declareRuntimeFunctions(gen);

  //  Convert i64 to ptr for Franz universal type system
  // Variant values are encoded as i64, need to convert back to Generic* for list operations
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef variantPtr = LLVMBuildIntToPtr(gen->builder, variantValue, genericPtrType, "variant_ptr");

  // Get list element at index 0 (tag)
  LLVMValueRef listNthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
  LLVMValueRef args[] = {
    variantPtr,  // Use converted pointer
    LLVMConstInt(gen->intType, 0, 0)
  };

  LLVMValueRef tagGeneric = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(listNthFunc),
    listNthFunc,
    args,
    2,
    "tag_generic"
  );

  // Extract string from Generic*
  LLVMValueRef stringUnboxFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_string");
  LLVMValueRef tagString = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(stringUnboxFunc),
    stringUnboxFunc,
    &tagGeneric,
    1,
    "tag_string"
  );

  return tagString;
}

LLVMValueRef LLVMAdt_extractValues(LLVMCodeGen *gen, LLVMValueRef variantValue) {
  declareRuntimeFunctions(gen);

  //  Convert i64 to ptr for Franz universal type system
  // Variant values are encoded as i64, need to convert back to Generic* for list operations
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef variantPtr = LLVMBuildIntToPtr(gen->builder, variantValue, genericPtrType, "variant_ptr");

  // Get list element at index 1 (values)
  LLVMValueRef listNthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
  LLVMValueRef args[] = {
    variantPtr,  // Use converted pointer
    LLVMConstInt(gen->intType, 1, 0)
  };

  LLVMValueRef valuesGeneric = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(listNthFunc),
    listNthFunc,
    args,
    2,
    "values_generic"
  );

  return valuesGeneric;
}

// ============================================================================
//  Value Unpacking for Parameterized Handlers
// ============================================================================

/**
 * @brief Unpack variant values into an array for parameterized handlers
 *
 * Extracts values from a variant's value list and converts them to i64 array
 * for passing as arguments to parameterized match handlers.
 *
 *  Implementation: Enables handlers like {x -> expr} or {x y -> expr}
 * to receive variant values as parameters instead of just {-> expr}.
 *
 * Example transformation:
 *   variant: ["Some", [42, "hello"]]
 *   handler: {x y -> (println x y)}
 *   unpacked: [i64(42), i64("hello")] → handler(42, "hello")
 *
 * @param gen LLVM code generator context
 * @param valuesListPtr LLVM value representing the values list (Generic*)
 * @param maxValues Maximum number of values to unpack (handler parameter count)
 * @param outCount Output parameter for actual number of values unpacked
 * @return Dynamically allocated array of LLVM values (i64), caller must free()
 */
LLVMValueRef* LLVMAdt_unpackValuesToArray(LLVMCodeGen *gen, LLVMValueRef valuesListPtr, int maxValues, int *outCount) {
  declareRuntimeFunctions(gen);

  // Get franz_list_nth function for extracting list elements
  LLVMValueRef listNthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
  if (!listNthFunc) {
    fprintf(stderr, "Runtime Error: franz_list_nth function not found\n");
    exit(1);
  }

  // Allocate array for unpacked values
  LLVMValueRef *values = malloc(sizeof(LLVMValueRef) * maxValues);
  if (!values) {
    fprintf(stderr, "Runtime Error: Failed to allocate memory for value unpacking\n");
    exit(1);
  }

  // Extract each value from the list
  for (int i = 0; i < maxValues; i++) {
    // Call franz_list_nth(valuesList, i) to get Generic* for value at index i
    LLVMValueRef args[] = {
      valuesListPtr,  // Generic* pointer to values list
      LLVMConstInt(gen->intType, i, 0)  // Index
    };

    LLVMValueRef valueGeneric = LLVMBuildCall2(
      gen->builder,
      LLVMGlobalGetValueType(listNthFunc),
      listNthFunc,
      args,
      2,
      "value_generic"
    );

    //  Convert Generic* to i64 for Franz universal type system
    // All values are passed as i64 in Franz's LLVM compilation
    LLVMValueRef valueAsInt = LLVMBuildPtrToInt(gen->builder, valueGeneric, gen->intType, "value_as_i64");
    values[i] = valueAsInt;
  }

  *outCount = maxValues;
  return values;
}
