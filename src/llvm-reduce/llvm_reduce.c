#include "llvm_reduce.h"
#include "../stdlib.h"
#include <stdio.h>

/**
 * LLVM Reduce Implementation
 *
 * Compiles reduce function to native code via LLVM.
 * reduce takes a list, a callback closure, and an optional initial accumulator,
 * returning a single reduced value.
 *
 * Strategy:
 * 1. Compile list, callback, and optional initial arguments
 * 2. Call runtime helper franz_llvm_reduce that:
 *    - Starts with initial accumulator (or void if not provided)
 *    - Iterates through list elements
 *    - Calls callback closure for each element with (acc, element, index)
 *    - Accumulates the result for the next iteration
 * 3. Return final accumulated value as Generic*
 *
 * This matches the pattern used by map/filter - runtime helper handles
 * the iteration and closure calling, LLVM just compiles the call.
 */

// Declare runtime reduce helper function
static void ensureRuntimeReduceFunction(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_llvm_reduce(Generic* list, Generic* callback, Generic* initial, int lineNumber) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_reduce")) {
    LLVMTypeRef params[] = {
      genericPtrType,  // list
      genericPtrType,  // callback closure
      genericPtrType,  // initial accumulator (or NULL)
      gen->intType     // lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 4, 0);
    LLVMAddFunction(gen->module, "franz_llvm_reduce", funcType);
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

// Declare runtime int boxing function
static void ensureBoxIntFunction(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_box_int(int64_t value) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_int")) {
    LLVMTypeRef params[] = { gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_int", funcType);
  }
}

LLVMValueRef LLVMReduce_compileReduce(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeReduceFunction(gen);
  ensureClosureBoxFunction(gen);
  ensureBoxIntFunction(gen);

  // Expect 2 or 3 arguments: (reduce list callback) or (reduce list callback initial)
  if (node->childCount < 2 || node->childCount > 3) {
    fprintf(stderr, "ERROR: reduce expects 2 or 3 arguments (list, callback [, initial]), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for reduce at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile callback argument
  LLVMValueRef callbackValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!callbackValue) {
    fprintf(stderr, "ERROR: Failed to compile callback argument for reduce at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Convert callback i64 to Generic* using franz_box_closure
  LLVMTypeRef callbackType = LLVMTypeOf(callbackValue);
  if (LLVMGetTypeKind(callbackType) == LLVMIntegerTypeKind) {
    // It's an i64 closure value - convert to void* then box
    LLVMValueRef closurePtr = LLVMBuildIntToPtr(gen->builder, callbackValue,
                                                 LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
                                                 "closure_ptr");

    // Call franz_box_closure(closurePtr) -> Generic*
    LLVMValueRef boxFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
    LLVMValueRef boxArgs[] = { closurePtr };
    callbackValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFunc),
                                    boxFunc, boxArgs, 1, "callback_boxed");
  }

  // Compile optional initial accumulator argument
  LLVMValueRef initialValue;
  if (node->childCount == 3) {
    initialValue = LLVMCodeGen_compileNode(gen, node->children[2]);
    if (!initialValue) {
      fprintf(stderr, "ERROR: Failed to compile initial argument for reduce at line %d\n",
              node->lineNumber);
      return NULL;
    }

    // Box the initial value if it's an integer
    LLVMTypeRef initialType = LLVMTypeOf(initialValue);
    if (LLVMGetTypeKind(initialType) == LLVMIntegerTypeKind) {
      LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
      LLVMValueRef boxArgs[] = { initialValue };
      initialValue = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                     boxIntFunc, boxArgs, 1, "initial_boxed");
    }
  } else {
    // No initial value provided - pass NULL (runtime will use void)
    initialValue = LLVMConstNull(LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0));
  }

  // Prepare line number argument
  LLVMValueRef lineNumberValue = LLVMConstInt(gen->intType, node->lineNumber, 0);

  // Call franz_llvm_reduce(listValue, callbackValue, initialValue, lineNumber)
  LLVMValueRef reduceFunc = LLVMGetNamedFunction(gen->module, "franz_llvm_reduce");
  LLVMValueRef args[] = { listValue, callbackValue, initialValue, lineNumberValue };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(reduceFunc),
                        reduceFunc, args, 4, "reduce_result");
}
