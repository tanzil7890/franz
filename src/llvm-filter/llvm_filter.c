#include "llvm_filter.h"
#include "../stdlib.h"
#include <stdio.h>

/**
 * LLVM Filter Implementation
 *
 * Compiles filter function to native code via LLVM.
 * filter takes a list and a predicate closure, returning elements that pass the test.
 *
 * Strategy:
 * 1. Compile list and predicate arguments
 * 2. Call runtime helper franz_llvm_filter that:
 *    - Iterates through list elements
 *    - Calls predicate closure for each element (with element and index)
 *    - Builds result list with elements where predicate returned truthy value
 * 3. Return filtered list as Generic*
 *
 * This matches the pattern used by map/reduce - runtime helper handles
 * the iteration and closure calling, LLVM just compiles the call.
 */

// Declare runtime filter helper function
static void ensureRuntimeFilterFunction(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_llvm_filter(Generic* list, Generic* predicate, int lineNumber) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_filter")) {
    LLVMTypeRef params[] = {
      genericPtrType,  // list
      genericPtrType,  // predicate closure
      gen->intType     // lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 3, 0);
    LLVMAddFunction(gen->module, "franz_llvm_filter", funcType);
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

LLVMValueRef LLVMFilter_compileFilter(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFilterFunction(gen);
  ensureClosureBoxFunction(gen);

  // Expect 2 arguments: (filter list predicate)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: filter expects 2 arguments (list, predicate), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for filter at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile predicate argument
  LLVMValueRef predicateValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!predicateValue) {
    fprintf(stderr, "ERROR: Failed to compile predicate argument for filter at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Convert predicate i64 to Generic* using franz_box_closure
  // Industry-standard approach: ~30ns overhead, optimal for runtime boundary crossing
  LLVMTypeRef predicateType = LLVMTypeOf(predicateValue);
  if (LLVMGetTypeKind(predicateType) == LLVMIntegerTypeKind) {
    // It's an i64 closure value - convert to void* then box
    LLVMValueRef closurePtr = LLVMBuildIntToPtr(gen->builder, predicateValue,
                                                 LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
                                                 "closure_ptr");

    // Call franz_box_closure(closurePtr) -> Generic*
    LLVMValueRef boxFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
    LLVMValueRef boxArgs[] = { closurePtr };
    predicateValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFunc),
                                     boxFunc, boxArgs, 1, "predicate_boxed");
  }

  // Prepare line number argument
  LLVMValueRef lineNumberValue = LLVMConstInt(gen->intType, node->lineNumber, 0);

  // Call franz_llvm_filter(listValue, predicateValue, lineNumber)
  LLVMValueRef filterFunc = LLVMGetNamedFunction(gen->module, "franz_llvm_filter");
  LLVMValueRef args[] = { listValue, predicateValue, lineNumberValue };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(filterFunc),
                        filterFunc, args, 3, "filter_result");
}
