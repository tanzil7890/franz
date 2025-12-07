#include "llvm_list_ops.h"
#include "../stdlib.h"
#include <stdio.h>

/**
 *  Industry-Standard LLVM List Operations
 *
 * Strategy: Compile list operations to LLVM IR that calls runtime helpers
 * This matches Rust's approach with Box<dyn Any> for heterogeneous collections
 */

// Declare runtime functions if not already declared
static void ensureRuntimeFunctions(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_list_head(Generic*) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_list_head")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_list_head", funcType);
  }

  // franz_list_tail(Generic*) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_list_tail")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_list_tail", funcType);
  }

  // franz_list_cons(Generic*, Generic*) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_list_cons")) {
    LLVMTypeRef params[] = { genericPtrType, genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 2, 0);
    LLVMAddFunction(gen->module, "franz_list_cons", funcType);
  }

  // franz_list_is_empty(Generic*) -> i64
  if (!LLVMGetNamedFunction(gen->module, "franz_list_is_empty")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_list_is_empty", funcType);
  }

  // franz_list_length(Generic*) -> i64
  if (!LLVMGetNamedFunction(gen->module, "franz_list_length")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_list_length", funcType);
  }

  // franz_list_nth(Generic*, i64) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_list_nth")) {
    LLVMTypeRef params[] = { genericPtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 2, 0);
    LLVMAddFunction(gen->module, "franz_list_nth", funcType);
  }

  // franz_is_list(Generic*) -> i64
  if (!LLVMGetNamedFunction(gen->module, "franz_is_list")) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "franz_is_list", funcType);
  }
}

LLVMValueRef LLVMListOps_compileHead(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFunctions(gen);

  // Expect 1 argument: (head list)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: head expects 1 argument, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for head at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call franz_list_head(listValue)
  LLVMValueRef headFunc = LLVMGetNamedFunction(gen->module, "franz_list_head");
  LLVMValueRef args[] = { listValue };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(headFunc),
                        headFunc, args, 1, "head");
}

LLVMValueRef LLVMListOps_compileTail(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFunctions(gen);

  // Expect 1 argument: (tail list)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: tail expects 1 argument, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for tail at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call franz_list_tail(listValue)
  LLVMValueRef tailFunc = LLVMGetNamedFunction(gen->module, "franz_list_tail");
  LLVMValueRef args[] = { listValue };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(tailFunc),
                        tailFunc, args, 1, "tail");
}

LLVMValueRef LLVMListOps_compileCons(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFunctions(gen);

  // Expect 2 arguments: (cons elem list)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: cons expects 2 arguments, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile element and list arguments
  LLVMValueRef elemValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!elemValue || !listValue) {
    fprintf(stderr, "ERROR: Failed to compile cons arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Box element if it's a native type (not already Generic*)
  LLVMTypeRef elemType = LLVMTypeOf(elemValue);
  LLVMTypeKind typeKind = LLVMGetTypeKind(elemType);
  LLVMValueRef boxedElem = elemValue;

  if (typeKind == LLVMIntegerTypeKind) {
    // Integer: box with franz_box_int
    LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
    LLVMValueRef boxArgs[] = { elemValue };
    boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                boxIntFunc, boxArgs, 1, "boxed_int");
  } else if (typeKind == LLVMDoubleTypeKind) {
    // Float: box with franz_box_float
    LLVMValueRef boxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_box_float");
    LLVMValueRef boxArgs[] = { elemValue };
    boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc),
                                boxFloatFunc, boxArgs, 1, "boxed_float");
  } else if (typeKind == LLVMPointerTypeKind) {
    // Check if it's a string literal (not already Generic*)
    AstNode *elemNode = node->children[0];
    if (elemNode->opcode == OP_STRING) {
      LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
      LLVMValueRef boxArgs[] = { elemValue };
      boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                  boxStringFunc, boxArgs, 1, "boxed_string");
    }
    // Otherwise assume it's already Generic* (from list operations, etc.)
  }

  // Call franz_list_cons(boxedElem, listValue)
  LLVMValueRef consFunc = LLVMGetNamedFunction(gen->module, "franz_list_cons");
  LLVMValueRef args[] = { boxedElem, listValue };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(consFunc),
                        consFunc, args, 2, "cons");
}

LLVMValueRef LLVMListOps_compileIsEmpty(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFunctions(gen);

  // Expect 1 argument: (empty? list)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: empty? expects 1 argument, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for empty? at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call franz_list_is_empty(listValue)
  LLVMValueRef isEmptyFunc = LLVMGetNamedFunction(gen->module, "franz_list_is_empty");
  LLVMValueRef args[] = { listValue };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(isEmptyFunc),
                        isEmptyFunc, args, 1, "is_empty");
}

LLVMValueRef LLVMListOps_compileLength(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFunctions(gen);

  // Expect 1 argument: (length list)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: length expects 1 argument, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list argument
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!listValue) {
    fprintf(stderr, "ERROR: Failed to compile list argument for length at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // CRITICAL: Check if listValue is i64 or ptr
  LLVMTypeRef listType = LLVMTypeOf(listValue);
  LLVMTypeKind listTypeKind = LLVMGetTypeKind(listType);

  fprintf(stderr, "[LENGTH DEBUG] listValue type kind: %d (8=IntegerTypeKind, 10=PointerTypeKind)\n", listTypeKind);

  // If it's i64 (from dict_keys or other operations), convert to ptr
  if (listTypeKind == LLVMIntegerTypeKind) {
    fprintf(stderr, "[LENGTH DEBUG] Converting i64 to ptr for franz_list_length\n");
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    listValue = LLVMBuildIntToPtr(gen->builder, listValue, genericPtrType, "list_ptr");
  }

  // Call franz_list_length(listValue)
  LLVMValueRef lengthFunc = LLVMGetNamedFunction(gen->module, "franz_list_length");
  LLVMValueRef args[] = { listValue };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(lengthFunc),
                        lengthFunc, args, 1, "length");
}

LLVMValueRef LLVMListOps_compileNth(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFunctions(gen);

  // Expect 2 arguments: (nth list index)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: nth expects 2 arguments, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile list and index arguments
  LLVMValueRef listValue = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef indexValue = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!listValue || !indexValue) {
    fprintf(stderr, "ERROR: Failed to compile nth arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call franz_list_nth(listValue, indexValue)
  LLVMValueRef nthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
  LLVMValueRef args[] = { listValue, indexValue };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(nthFunc),
                        nthFunc, args, 2, "nth");
}
