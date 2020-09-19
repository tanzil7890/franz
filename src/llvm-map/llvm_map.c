#include "llvm_map.h"
#include "../stdlib.h"
#include <stdio.h>

/**
 * LLVM Map Implementation
 *
 * Compiles map function to native code via LLVM.
 * map takes a list and a callback closure, returning a new list with transformed elements.
 *
 * Strategy:
 * 1. Compile list and callback arguments
 * 2. Call runtime helper franz_llvm_map that:
 *    - Iterates through list elements
 *    - Calls callback closure for each element (with element and index)
 *    - Builds result list with all transformed elements
 * 3. Return transformed list as Generic*
 *
 * This matches the pattern used by filter - runtime helper handles
 * the iteration and closure calling, LLVM just compiles the call.
 */

// Declare runtime map helper function
static void ensureRuntimeMapFunction(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_llvm_map(Generic* list, Generic* callback, int lineNumber) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_map")) {
    LLVMTypeRef params[] = {
      genericPtrType,  // list
      genericPtrType,  // callback closure
      gen->intType     // lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 3, 0);
    LLVMAddFunction(gen->module, "franz_llvm_map", funcType);
  }
}

// Declare runtime closure boxing function (wraps BytecodeClosureData* in Generic*)
static void ensureClosureBoxFunction(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMTypeRef voidPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_box_closure(void* closure_ptr) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_closure")) {
    LLVMTypeRef params[] = { voidPtrType };  // void* (BytecodeClosureData*)
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_closure", funcType);
  }
}

LLVMValueRef LLVMMap_compileMap(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeMapFunction(gen);
  ensureClosureBoxFunction(gen);

  // Expect 2 arguments: (map list callback)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: map expects 2 arguments (list, callback), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for map at line %d\n",
            node->lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Compile callback argument
  LLVMValueRef callbackValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!callbackValue) {
    fprintf(stderr, "ERROR: Failed to compile callback argument for map at line %d\n",
            node->lineNumber);
    return NULL;
  }

  int callbackIsGeneric = isGenericPointerNode(gen, node->children[1]);
  LLVMTypeRef callbackType = LLVMTypeOf(callbackValue);

  if (callbackIsGeneric) {
    // Already a Generic* (e.g., closure returned from another closure)
    if (LLVMGetTypeKind(callbackType) == LLVMIntegerTypeKind) {
      callbackValue = LLVMBuildIntToPtr(gen->builder, callbackValue, genericPtrType,
                                        "map_callback_generic_ptr");
    }
  } else {
    // Raw closure pointer - convert to Generic* via franz_box_closure
    if (LLVMGetTypeKind(callbackType) == LLVMIntegerTypeKind) {
      LLVMValueRef closurePtr = LLVMBuildIntToPtr(gen->builder, callbackValue,
                                                   genericPtrType,
                                                   "closure_ptr");

      LLVMValueRef boxFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
      LLVMValueRef boxArgs[] = { closurePtr };
      callbackValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFunc),
                                      boxFunc, boxArgs, 1, "callback_boxed");
    }
  }

  // Prepare line number argument
  LLVMValueRef lineNumberValue = LLVMConstInt(gen->intType, node->lineNumber, 0);

  // Call franz_llvm_map(listValue, callbackValue, lineNumber)
  LLVMValueRef mapFunc = LLVMGetNamedFunction(gen->module, "franz_llvm_map");
  LLVMValueRef args[] = { listValue, callbackValue, lineNumberValue };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(mapFunc),
                        mapFunc, args, 3, "map_result");
}

// Declare runtime map2 helper function
static void ensureRuntimeMap2Function(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_llvm_map2(Generic* list1, Generic* list2, Generic* callback, int lineNumber) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_map2")) {
    LLVMTypeRef params[] = {
      genericPtrType,  // list1
      genericPtrType,  // list2
      genericPtrType,  // callback closure
      gen->intType     // lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 4, 0);
    LLVMAddFunction(gen->module, "franz_llvm_map2", funcType);
  }
}

LLVMValueRef LLVMMap_compileMap2(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeMap2Function(gen);
  ensureClosureBoxFunction(gen);

  // Expect 3 arguments: (map2 list1 list2 callback)
  if (node->childCount != 3) {
    fprintf(stderr, "ERROR: map2 expects 3 arguments (list1, list2, callback), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list1 argument
  LLVMValueRef list1Value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!list1Value) {
    fprintf(stderr, "ERROR: Failed to compile list1 argument for map2 at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile list2 argument
  LLVMValueRef list2Value = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!list2Value) {
    fprintf(stderr, "ERROR: Failed to compile list2 argument for map2 at line %d\n",
            node->lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Compile callback argument
  LLVMValueRef callbackValue = LLVMCodeGen_compileNode(gen, node->children[2]);
  if (!callbackValue) {
    fprintf(stderr, "ERROR: Failed to compile callback argument for map2 at line %d\n",
            node->lineNumber);
    return NULL;
  }

  int callbackIsGeneric = isGenericPointerNode(gen, node->children[2]);
  LLVMTypeRef callbackType = LLVMTypeOf(callbackValue);

  if (callbackIsGeneric) {
    if (LLVMGetTypeKind(callbackType) == LLVMIntegerTypeKind) {
      callbackValue = LLVMBuildIntToPtr(gen->builder, callbackValue, genericPtrType,
                                        "map2_callback_generic_ptr");
    }
  } else {
    if (LLVMGetTypeKind(callbackType) == LLVMIntegerTypeKind) {
      LLVMValueRef closurePtr = LLVMBuildIntToPtr(gen->builder, callbackValue,
                                                   genericPtrType,
                                                   "closure_ptr");

      LLVMValueRef boxFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
      LLVMValueRef boxArgs[] = { closurePtr };
      callbackValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFunc),
                                      boxFunc, boxArgs, 1, "callback_boxed");
    }
  }

  // Prepare line number argument
  LLVMValueRef lineNumberValue = LLVMConstInt(gen->intType, node->lineNumber, 0);

  // Call franz_llvm_map2(list1Value, list2Value, callbackValue, lineNumber)
  LLVMValueRef map2Func = LLVMGetNamedFunction(gen->module, "franz_llvm_map2");
  LLVMValueRef args[] = { list1Value, list2Value, callbackValue, lineNumberValue };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(map2Func),
                        map2Func, args, 4, "map2_result");
}
