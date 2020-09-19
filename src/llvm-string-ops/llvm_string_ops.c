#include "llvm_string_ops.h"
#include "../llvm-unboxing/llvm_unboxing.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// LLVM-Native Get Function (String/List Indexing and Slicing)
// ============================================================================

/**
 * Compile (get collection index) or (get collection start end)
 *
 * Strategy:
 * 1. Unbox Generic* to check runtime type (TYPE_STRING or TYPE_LIST)
 * 2. Branch based on type:
 *    - TYPE_STRING: LLVM-native substring extraction (pure LLVM IR)
 *    - TYPE_LIST: Call franz_list_nth for element access
 * 3. Return result (char* for strings, Generic* for lists)
 *
 * Performance Characteristics:
 * - String single char: ~15 LLVM instructions (malloc, GEP, load, store)
 * - String substring: ~25 LLVM instructions + loop (still C-level speed)
 * - List element: Direct memory access via franz_list_nth
 */
LLVMValueRef LLVMStringOps_compileGet(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 2 || node->childCount > 3) {
    fprintf(stderr, "ERROR: get requires 2 or 3 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile collection (first argument)
  LLVMValueRef collection = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!collection) {
    fprintf(stderr, "ERROR: Failed to compile get collection argument at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile start index (second argument)
  LLVMValueRef start = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!start) {
    fprintf(stderr, "ERROR: Failed to compile get start index at line %d\n", node->lineNumber);
    return NULL;
  }

  // Check the type of collection
  LLVMTypeRef collectionType = LLVMTypeOf(collection);
  LLVMTypeKind collectionKind = LLVMGetTypeKind(collectionType);
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  //  Distinguish raw string literals (global constants) from Generic* values
  // Use LLVMIsAGlobalVariable to detect string literals
  int isGlobalString = (collectionKind == LLVMPointerTypeKind &&
                       LLVMIsAGlobalVariable(collection) != NULL);

  // ========== FAST PATH: Raw String Literal (Global Constant) ==========
  if (isGlobalString) {
    // Raw string literal (i8*), not wrapped in Generic* - process directly
    LLVMValueRef stringPtr = collection;
    LLVMValueRef stringResult;

    if (node->childCount == 2) {
      // Single character
      LLVMValueRef two = LLVMConstInt(gen->intType, 2, 0);
      LLVMValueRef mallocArgs[] = {two};
      LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                          mallocArgs, 1, "char_buffer");
      LLVMValueRef charPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                          stringPtr, &start, 1, "char_ptr");
      LLVMValueRef charValue = LLVMBuildLoad2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             charPtr, "char_value");
      LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
      LLVMValueRef bufferCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                                 buffer, &zero, 1, "buffer_char_ptr");
      LLVMBuildStore(gen->builder, charValue, bufferCharPtr);
      LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);
      LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                          buffer, &one, 1, "null_ptr");
      LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
      LLVMBuildStore(gen->builder, nullChar, nullPtr);
      stringResult = buffer;
    } else {
      // Substring
      LLVMValueRef end = LLVMCodeGen_compileNode(gen, node->children[2]);
      if (!end) return NULL;
      LLVMValueRef length = LLVMBuildSub(gen->builder, end, start, "sub_length");
      LLVMValueRef bufferSize = LLVMBuildAdd(gen->builder, length,
                                            LLVMConstInt(gen->intType, 1, 0), "buffer_size");
      LLVMValueRef mallocArgs[] = {bufferSize};
      LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                          mallocArgs, 1, "substring_buffer");

      // Runtime loop to copy characters
      LLVMBasicBlockRef loopHead = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "loop_head");
      LLVMBasicBlockRef loopBody = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "loop_body");
      LLVMBasicBlockRef loopEnd = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "loop_end");

      LLVMValueRef counter = LLVMBuildAlloca(gen->builder, gen->intType, "counter");
      LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
      LLVMBuildStore(gen->builder, zero, counter);
      LLVMBuildBr(gen->builder, loopHead);

      LLVMPositionBuilderAtEnd(gen->builder, loopHead);
      LLVMValueRef i = LLVMBuildLoad2(gen->builder, gen->intType, counter, "i");
      LLVMValueRef cond = LLVMBuildICmp(gen->builder, LLVMIntSLT, i, length, "loop_cond");
      LLVMBuildCondBr(gen->builder, cond, loopBody, loopEnd);

      LLVMPositionBuilderAtEnd(gen->builder, loopBody);
      LLVMValueRef srcIndex = LLVMBuildAdd(gen->builder, start, i, "src_index");
      LLVMValueRef srcCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             stringPtr, &srcIndex, 1, "src_char_ptr");
      LLVMValueRef charValue = LLVMBuildLoad2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             srcCharPtr, "char_value");
      LLVMValueRef dstCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             buffer, &i, 1, "dst_char_ptr");
      LLVMBuildStore(gen->builder, charValue, dstCharPtr);
      LLVMValueRef next = LLVMBuildAdd(gen->builder, i, LLVMConstInt(gen->intType, 1, 0), "next");
      LLVMBuildStore(gen->builder, next, counter);
      LLVMBuildBr(gen->builder, loopHead);

      LLVMPositionBuilderAtEnd(gen->builder, loopEnd);
      LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                          buffer, &length, 1, "null_ptr");
      LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
      LLVMBuildStore(gen->builder, nullChar, nullPtr);
      stringResult = buffer;
    }

    // Box the result
    LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
    if (!boxStringFunc) {
      LLVMTypeRef boxStringParams[] = {gen->stringType};
      LLVMTypeRef boxStringType = LLVMFunctionType(genericPtrType, boxStringParams, 1, 0);
      boxStringFunc = LLVMAddFunction(gen->module, "franz_box_string", boxStringType);
    }
    LLVMValueRef boxArgs[] = {stringResult};
    return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                         boxStringFunc, boxArgs, 1, "string_result_boxed");
  }

  // ========== SLOW PATH: Generic* (from dict, variables, etc.) ==========
  // Convert i64 to ptr if needed
  if (collectionKind == LLVMIntegerTypeKind) {
    collection = LLVMBuildIntToPtr(gen->builder, collection, genericPtrType, "collection_ptr");
  }

  // Use runtime functions to get type and p_val from Generic*
  // franz_generic_get_type(Generic*) -> i32
  LLVMValueRef getTypeFunc = LLVMGetNamedFunction(gen->module, "franz_generic_get_type");
  if (!getTypeFunc) {
    LLVMTypeRef getTypeParams[] = {genericPtrType};
    LLVMTypeRef getTypeType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context), getTypeParams, 1, 0);
    getTypeFunc = LLVMAddFunction(gen->module, "franz_generic_get_type", getTypeType);
  }

  // franz_generic_get_pval(Generic*) -> void*
  LLVMValueRef getPvalFunc = LLVMGetNamedFunction(gen->module, "franz_generic_get_pval");
  if (!getPvalFunc) {
    LLVMTypeRef getPvalParams[] = {genericPtrType};
    LLVMTypeRef getPvalType = LLVMFunctionType(genericPtrType, getPvalParams, 1, 0);
    getPvalFunc = LLVMAddFunction(gen->module, "franz_generic_get_pval", getPvalType);
  }

  // Call runtime functions
  LLVMValueRef typeArgs[] = {collection};
  LLVMValueRef typeValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(getTypeFunc),
                                         getTypeFunc, typeArgs, 1, "type_value");

  LLVMValueRef pvalArgs[] = {collection};
  LLVMValueRef pValPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(getPvalFunc),
                                       getPvalFunc, pvalArgs, 1, "p_val_ptr");

  // Create basic blocks for type branching
  LLVMBasicBlockRef stringBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "get_string");
  LLVMBasicBlockRef listBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "get_list");
  LLVMBasicBlockRef errorBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "get_error");
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "get_merge");

  // TYPE_STRING = 2, TYPE_LIST = 6 (from enum Type in generic.h)
  // enum Type { TYPE_INT=0, TYPE_FLOAT=1, TYPE_STRING=2, TYPE_VOID=3, TYPE_FUNCTION=4, TYPE_NATIVEFUNCTION=5, TYPE_LIST=6, ... }
  LLVMValueRef isString = LLVMBuildICmp(gen->builder, LLVMIntEQ, typeValue,
                                       LLVMConstInt(LLVMInt32TypeInContext(gen->context), 2, 0),
                                       "is_string");
  LLVMBasicBlockRef typeCheckBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_check");
  LLVMBuildCondBr(gen->builder, isString, stringBlock, typeCheckBlock);

  // Type check block: check if list
  LLVMPositionBuilderAtEnd(gen->builder, typeCheckBlock);
  LLVMValueRef isList = LLVMBuildICmp(gen->builder, LLVMIntEQ, typeValue,
                                     LLVMConstInt(LLVMInt32TypeInContext(gen->context), 6, 0),
                                     "is_list");
  LLVMBuildCondBr(gen->builder, isList, listBlock, errorBlock);

  // ========== STRING BLOCK: LLVM-Native Substring Extraction ==========
  LLVMPositionBuilderAtEnd(gen->builder, stringBlock);

  // pValPtr points to char** (double pointer for strings)
  // Dereference to get char*
  LLVMValueRef stringPtrPtr = LLVMBuildBitCast(gen->builder, pValPtr,
                                               LLVMPointerType(gen->stringType, 0),
                                               "string_ptr_ptr");
  LLVMValueRef stringPtr = LLVMBuildLoad2(gen->builder, gen->stringType,
                                         stringPtrPtr, "string_ptr");

  LLVMValueRef stringResult;
  if (node->childCount == 2) {
    // Single character: (get "hello" 0) → "h"
    LLVMValueRef two = LLVMConstInt(gen->intType, 2, 0);
    LLVMValueRef mallocArgs[] = {two};
    LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                        mallocArgs, 1, "char_buffer");

    // Load character at index
    LLVMValueRef charPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                        stringPtr, &start, 1, "char_ptr");
    LLVMValueRef charValue = LLVMBuildLoad2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                           charPtr, "char_value");

    // Store character in buffer[0]
    LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
    LLVMValueRef bufferCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                               buffer, &zero, 1, "buffer_char_ptr");
    LLVMBuildStore(gen->builder, charValue, bufferCharPtr);

    // Store null terminator in buffer[1]
    LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);
    LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                        buffer, &one, 1, "null_ptr");
    LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
    LLVMBuildStore(gen->builder, nullChar, nullPtr);

    stringResult = buffer;
  } else {
    // Substring: (get "hello" 0 3) → "hel"
    LLVMValueRef end = LLVMCodeGen_compileNode(gen, node->children[2]);
    if (!end) {
      fprintf(stderr, "ERROR: Failed to compile get end argument at line %d\n", node->lineNumber);
      return NULL;
    }

    // Calculate substring length: end - start
    LLVMValueRef length = LLVMBuildSub(gen->builder, end, start, "sub_length");

    // Allocate buffer: length + 1 (for null terminator)
    LLVMValueRef bufferSize = LLVMBuildAdd(gen->builder, length,
                                          LLVMConstInt(gen->intType, 1, 0), "buffer_size");
    LLVMValueRef mallocArgs[] = {bufferSize};
    LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                        mallocArgs, 1, "substring_buffer");

    // Copy characters using loop
    LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "substr_loop");
    LLVMBasicBlockRef loopBody = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "substr_body");
    LLVMBasicBlockRef loopEnd = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "substr_end");

    // Loop counter
    LLVMValueRef counter = LLVMBuildAlloca(gen->builder, gen->intType, "loop_counter");
    LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
    LLVMBuildStore(gen->builder, zero, counter);
    LLVMBuildBr(gen->builder, loopBlock);

    // Loop condition
    LLVMPositionBuilderAtEnd(gen->builder, loopBlock);
    LLVMValueRef i = LLVMBuildLoad2(gen->builder, gen->intType, counter, "i");
    LLVMValueRef cond = LLVMBuildICmp(gen->builder, LLVMIntSLT, i, length, "loop_cond");
    LLVMBuildCondBr(gen->builder, cond, loopBody, loopEnd);

    // Loop body: copy character
    LLVMPositionBuilderAtEnd(gen->builder, loopBody);
    LLVMValueRef srcIndex = LLVMBuildAdd(gen->builder, start, i, "src_index");
    LLVMValueRef srcCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                           stringPtr, &srcIndex, 1, "src_char_ptr");
    LLVMValueRef charValue = LLVMBuildLoad2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                           srcCharPtr, "char_value");

    LLVMValueRef dstCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                           buffer, &i, 1, "dst_char_ptr");
    LLVMBuildStore(gen->builder, charValue, dstCharPtr);

    // Increment counter
    LLVMValueRef next = LLVMBuildAdd(gen->builder, i, LLVMConstInt(gen->intType, 1, 0), "next");
    LLVMBuildStore(gen->builder, next, counter);
    LLVMBuildBr(gen->builder, loopBlock);

    // Loop end: add null terminator
    LLVMPositionBuilderAtEnd(gen->builder, loopEnd);
    LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                        buffer, &length, 1, "null_ptr");
    LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
    LLVMBuildStore(gen->builder, nullChar, nullPtr);

    stringResult = buffer;
  }

  // Box the string result into Generic* using franz_box_string
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
  if (!boxStringFunc) {
    LLVMTypeRef boxStringParams[] = {gen->stringType};
    LLVMTypeRef boxStringType = LLVMFunctionType(genericPtrType, boxStringParams, 1, 0);
    boxStringFunc = LLVMAddFunction(gen->module, "franz_box_string", boxStringType);
  }

  LLVMValueRef boxArgs[] = {stringResult};
  LLVMValueRef stringResultBoxed = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                                   boxStringFunc, boxArgs, 1, "string_result_boxed");
  LLVMBuildBr(gen->builder, mergeBlock);
  LLVMBasicBlockRef stringExitBlock = LLVMGetInsertBlock(gen->builder);

  // ========== LIST BLOCK: Call franz_list_nth ==========
  LLVMPositionBuilderAtEnd(gen->builder, listBlock);

  // Call franz_list_nth(Generic* list, i64 index)
  LLVMValueRef listNthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
  if (!listNthFunc) {
    LLVMTypeRef listNthParams[] = {genericPtrType, gen->intType};
    LLVMTypeRef listNthType = LLVMFunctionType(genericPtrType, listNthParams, 2, 0);
    listNthFunc = LLVMAddFunction(gen->module, "franz_list_nth", listNthType);
  }

  LLVMValueRef listArgs[] = {collection, start};
  LLVMValueRef listResult = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNthFunc),
                                          listNthFunc, listArgs, 2, "list_nth_result");
  LLVMBuildBr(gen->builder, mergeBlock);
  LLVMBasicBlockRef listExitBlock = LLVMGetInsertBlock(gen->builder);

  // ========== ERROR BLOCK ==========
  LLVMPositionBuilderAtEnd(gen->builder, errorBlock);
  // Just return null for now (could add runtime error message)
  LLVMValueRef nullResult = LLVMConstNull(genericPtrType);
  LLVMBuildBr(gen->builder, mergeBlock);
  LLVMBasicBlockRef errorExitBlock = LLVMGetInsertBlock(gen->builder);

  // ========== MERGE BLOCK: PHI Node ==========
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, genericPtrType, "get_result");

  LLVMValueRef phiValues[] = {stringResultBoxed, listResult, nullResult};
  LLVMBasicBlockRef phiBlocks[] = {stringExitBlock, listExitBlock, errorExitBlock};
  LLVMAddIncoming(phi, phiValues, phiBlocks, 3);

  // Convert Generic* to i64 for Universal Type System (like dict_get does)
  LLVMValueRef resultAsInt = LLVMBuildPtrToInt(gen->builder, phi, gen->intType, "get_result_i64");

  return resultAsInt;
}
