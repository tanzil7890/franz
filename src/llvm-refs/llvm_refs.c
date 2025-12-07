#include "llvm_refs.h"
#include <stdio.h>
#include <string.h>

/**
 *  LLVM Mutable References Implementation
 *
 * Implements OCaml-style mutable references with C-level performance:
 * - ref:   Create mutable reference box (heap allocation)
 * - deref: Read current value (load from heap)
 * - set!:  Update reference (store to heap)
 *
 * Runtime helpers in stdlib.c:
 * - franz_llvm_create_ref(Generic* value, int lineNumber) -> Generic* (TYPE_REF)
 * - franz_llvm_deref(Generic* ref, int lineNumber) -> Generic* (stored value)
 * - franz_llvm_set_ref(Generic* ref, Generic* value, int lineNumber) -> Generic* (TYPE_VOID)
 *
 * Pattern: Box native LLVM values (i64, double, i8*) into Generic* pointers
 * before passing to runtime functions. Similar to variant/ADT implementation.
 */

// Forward declaration
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);

// Helper to get Generic* pointer type (i8* in LLVM 17 opaque pointers)
static LLVMTypeRef getGenericPtrType(LLVMCodeGen *gen) {
  return LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
}

// Declare boxing functions if needed
static void ensureBoxingFunctions(LLVMCodeGen *gen) {
  LLVMTypeRef genericPtrType = getGenericPtrType(gen);

  // franz_box_int(i64) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_int")) {
    LLVMTypeRef paramTypes[] = { gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, paramTypes, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_int", funcType);
  }

  // franz_box_float(double) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_float")) {
    LLVMTypeRef paramTypes[] = { gen->floatType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, paramTypes, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_float", funcType);
  }

  // franz_box_string(i8*) -> Generic*
  if (!LLVMGetNamedFunction(gen->module, "franz_box_string")) {
    LLVMTypeRef paramTypes[] = { gen->stringType };
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, paramTypes, 1, 0);
    LLVMAddFunction(gen->module, "franz_box_string", funcType);
  }
}

/**
 * Box a value into Generic* based on its LLVM type
 * Returns Generic* pointer suitable for runtime functions
 */
static LLVMValueRef boxValue(LLVMCodeGen *gen, LLVMValueRef value, AstNode *node) {
  ensureBoxingFunctions(gen);

  LLVMTypeRef valueType = LLVMTypeOf(value);
  LLVMTypeKind typeKind = LLVMGetTypeKind(valueType);

  // If already a pointer and from a node that produces Generic*, return as-is
  if (typeKind == LLVMPointerTypeKind &&
      (node->opcode == OP_APPLICATION || node->opcode == OP_IDENTIFIER)) {
    return value;
  }

  // Integer: box with franz_box_int
  if (typeKind == LLVMIntegerTypeKind && valueType == gen->intType) {
    LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
    LLVMValueRef args[] = { value };
    return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                          boxIntFunc, args, 1, "boxed_int");
  }

  // Float: box with franz_box_float
  if (typeKind == LLVMDoubleTypeKind) {
    LLVMValueRef boxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_box_float");
    LLVMValueRef args[] = { value };
    return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc),
                          boxFloatFunc, args, 1, "boxed_float");
  }

  // String: box with franz_box_string
  if (typeKind == LLVMPointerTypeKind && node->opcode == OP_STRING) {
    LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
    LLVMValueRef args[] = { value };
    return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                          boxStringFunc, args, 1, "boxed_string");
  }

  // Pointer (already Generic*)
  if (typeKind == LLVMPointerTypeKind) {
    return value;
  }

  fprintf(stderr, "ERROR: Unsupported value type for boxing (typeKind=%d) at line %d\n",
          typeKind, node->lineNumber);
  return NULL;
}

/**
 * Compile (ref value) - Create mutable reference
 *
 * Strategy:
 * 1. Declare franz_llvm_create_ref runtime function
 * 2. Compile initial value expression
 * 3. Box value into Generic* pointer
 * 4. Call franz_llvm_create_ref(value, lineNumber)
 * 5. Return Generic* (TYPE_REF)
 */
LLVMValueRef LLVMCodeGen_compileRef(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: ref requires exactly 1 argument (initial value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Get Generic* pointer type
  LLVMTypeRef genericPtrType = getGenericPtrType(gen);

  // Declare franz_llvm_create_ref if not already declared
  // Generic *franz_llvm_create_ref(Generic *value, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_create_ref")) {
    LLVMTypeRef paramTypes[] = {
      genericPtrType,   // Generic* value
      gen->intType      // int lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(
      genericPtrType,   // Returns Generic*
      paramTypes,
      2,                // 2 parameters
      0                 // Not variadic
    );
    LLVMAddFunction(gen->module, "franz_llvm_create_ref", funcType);
  }

  // Compile the initial value expression
  AstNode *valueNode = node->children[0];
  LLVMValueRef valueResult = LLVMCodeGen_compileNode(gen, valueNode);
  if (!valueResult) {
    fprintf(stderr, "ERROR: Failed to compile ref argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Box the value into Generic*
  LLVMValueRef boxedValue = boxValue(gen, valueResult, valueNode);
  if (!boxedValue) {
    fprintf(stderr, "ERROR: Failed to box ref argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Prepare arguments for franz_llvm_create_ref
  LLVMValueRef args[] = {
    boxedValue,                                        // Generic* value
    LLVMConstInt(gen->intType, node->lineNumber, 0)   // int lineNumber
  };

  // Call franz_llvm_create_ref(value, lineNumber)
  LLVMValueRef createRefFunc = LLVMGetNamedFunction(gen->module, "franz_llvm_create_ref");
  LLVMValueRef refResult = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(createRefFunc),
    createRefFunc,
    args,
    2,
    "ref_result"
  );

  // Result is Generic* (TYPE_REF)
  return refResult;
}

/**
 * Compile (deref ref) - Dereference (read value)
 *
 * Strategy:
 * 1. Declare franz_llvm_deref runtime function
 * 2. Compile reference expression (should be Generic*)
 * 3. Call franz_llvm_deref(ref, lineNumber)
 * 4. Return Generic* (type depends on stored value)
 */
LLVMValueRef LLVMCodeGen_compileDeref(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: deref requires exactly 1 argument (reference) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Get Generic* pointer type
  LLVMTypeRef genericPtrType = getGenericPtrType(gen);

  // Declare franz_llvm_deref if not already declared
  // Generic *franz_llvm_deref(Generic *ref, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_deref")) {
    LLVMTypeRef paramTypes[] = {
      genericPtrType,   // Generic* ref
      gen->intType      // int lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(
      genericPtrType,   // Returns Generic*
      paramTypes,
      2,                // 2 parameters
      0                 // Not variadic
    );
    LLVMAddFunction(gen->module, "franz_llvm_deref", funcType);
  }

  // Compile the reference expression
  AstNode *refNode = node->children[0];
  LLVMValueRef refResult = LLVMCodeGen_compileNode(gen, refNode);
  if (!refResult) {
    fprintf(stderr, "ERROR: Failed to compile deref argument at line %d\n", refNode->lineNumber);
    return NULL;
  }

  // Reference should be Generic* (TYPE_REF) - already boxed from (ref ...) call
  // Prepare arguments for franz_llvm_deref
  LLVMValueRef args[] = {
    refResult,                                         // Generic* ref
    LLVMConstInt(gen->intType, node->lineNumber, 0)   // int lineNumber
  };

  // Call franz_llvm_deref(ref, lineNumber)
  LLVMValueRef derefFunc = LLVMGetNamedFunction(gen->module, "franz_llvm_deref");
  LLVMValueRef derefResult = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(derefFunc),
    derefFunc,
    args,
    2,
    "deref_result"
  );

  // Result is Generic* (type depends on stored value)
  return derefResult;
}

/**
 * Compile (set! ref new-value) - Update reference
 *
 * Strategy:
 * 1. Declare franz_llvm_set_ref runtime function
 * 2. Compile reference expression (should be Generic*)
 * 3. Compile new value expression
 * 4. Box new value into Generic* pointer
 * 5. Call franz_llvm_set_ref(ref, value, lineNumber)
 * 6. Return Generic* (TYPE_VOID)
 */
LLVMValueRef LLVMCodeGen_compileSetRef(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: set! requires exactly 2 arguments (reference, new-value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Get Generic* pointer type
  LLVMTypeRef genericPtrType = getGenericPtrType(gen);

  // Declare franz_llvm_set_ref if not already declared
  // Generic *franz_llvm_set_ref(Generic *ref, Generic *value, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "franz_llvm_set_ref")) {
    LLVMTypeRef paramTypes[] = {
      genericPtrType,   // Generic* ref
      genericPtrType,   // Generic* value
      gen->intType      // int lineNumber
    };
    LLVMTypeRef funcType = LLVMFunctionType(
      genericPtrType,   // Returns Generic* (TYPE_VOID)
      paramTypes,
      3,                // 3 parameters
      0                 // Not variadic
    );
    LLVMAddFunction(gen->module, "franz_llvm_set_ref", funcType);
  }

  // Compile the reference expression (first argument)
  AstNode *refNode = node->children[0];
  LLVMValueRef refResult = LLVMCodeGen_compileNode(gen, refNode);
  if (!refResult) {
    fprintf(stderr, "ERROR: Failed to compile set! reference argument at line %d\n", refNode->lineNumber);
    return NULL;
  }

  // Compile the new value expression (second argument)
  AstNode *valueNode = node->children[1];
  LLVMValueRef valueResult = LLVMCodeGen_compileNode(gen, valueNode);
  if (!valueResult) {
    fprintf(stderr, "ERROR: Failed to compile set! value argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Box the new value into Generic*
  LLVMValueRef boxedValue = boxValue(gen, valueResult, valueNode);
  if (!boxedValue) {
    fprintf(stderr, "ERROR: Failed to box set! value argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Prepare arguments for franz_llvm_set_ref
  LLVMValueRef args[] = {
    refResult,                                         // Generic* ref
    boxedValue,                                        // Generic* value
    LLVMConstInt(gen->intType, node->lineNumber, 0)   // int lineNumber
  };

  // Call franz_llvm_set_ref(ref, value, lineNumber)
  LLVMValueRef setRefFunc = LLVMGetNamedFunction(gen->module, "franz_llvm_set_ref");
  LLVMValueRef setResult = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(setRefFunc),
    setRefFunc,
    args,
    3,
    ""  // set! returns void, no name needed
  );

  // Result is Generic* (TYPE_VOID)
  return setResult;
}
