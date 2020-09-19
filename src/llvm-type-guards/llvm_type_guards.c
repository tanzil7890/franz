#include "llvm_type_guards.h"
#include <stdio.h>
#include <string.h>

/**
 *  Type Guard Functions
 *
 * Implements compile-time type checking predicates for LLVM values:
 * - is_int:    Check if value is i64 (integer)
 * - is_float:  Check if value is double (float)
 * - is_string: Check if value is i8* (string pointer)
 *
 * Strategy:
 * - Use LLVMTypeOf() to get LLVM type of value
 * - Use LLVMGetTypeKind() to check type kind
 * - Return constant 1 or 0 based on type match
 * - Zero runtime overhead (constant folding by LLVM)
 */

// Forward declaration
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile is_int type guard
 *
 * Syntax: (is_int value)
 *
 * Behavior:
 * - Compiles value expression
 * - Checks if LLVM type is IntegerType with 64 bits
 * - Returns constant 1 (true) or 0 (false)
 *
 * LLVM Implementation:
 * - Get type: LLVMTypeOf(value)
 * - Check kind: LLVMGetTypeKind(type) == LLVMIntegerTypeKind
 * - Check width: LLVMGetIntTypeWidth(type) == 64
 * - Return: LLVMConstInt(i64, match ? 1 : 0)
 *
 * Example:
 *   (is_int 42)          → 1 (compile-time constant)
 *   (is_int 3.14)        → 0 (compile-time constant)
 *   (is_int "hello")     → 0 (compile-time constant)
 */
LLVMValueRef LLVMCodeGen_compileIsInt(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: is_int requires exactly 1 argument (value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile the value expression
  AstNode *valueNode = node->children[0];
  LLVMValueRef value = LLVMCodeGen_compileNode(gen, valueNode);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile is_int argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Get LLVM type of the value
  LLVMTypeRef valueType = LLVMTypeOf(value);
  LLVMTypeKind typeKind = LLVMGetTypeKind(valueType);

  // Check if it's an integer type with 64 bits
  int isInt = 0;
  if (typeKind == LLVMIntegerTypeKind) {
    unsigned int bitWidth = LLVMGetIntTypeWidth(valueType);
    if (bitWidth == 64) {
      isInt = 1;
    }
  }

  // Return constant 1 or 0
  return LLVMConstInt(gen->intType, isInt ? 1 : 0, 0);
}

/**
 * Compile is_float type guard
 *
 * Syntax: (is_float value)
 *
 * Behavior:
 * - Compiles value expression
 * - Checks if LLVM type is DoubleType
 * - Returns constant 1 (true) or 0 (false)
 *
 * LLVM Implementation:
 * - Get type: LLVMTypeOf(value)
 * - Check kind: LLVMGetTypeKind(type) == LLVMDoubleTypeKind
 * - Return: LLVMConstInt(i64, match ? 1 : 0)
 *
 * Example:
 *   (is_float 3.14)      → 1 (compile-time constant)
 *   (is_float 42)        → 0 (compile-time constant)
 *   (is_float "hello")   → 0 (compile-time constant)
 */
LLVMValueRef LLVMCodeGen_compileIsFloat(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: is_float requires exactly 1 argument (value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile the value expression
  AstNode *valueNode = node->children[0];
  LLVMValueRef value = LLVMCodeGen_compileNode(gen, valueNode);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile is_float argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Get LLVM type of the value
  LLVMTypeRef valueType = LLVMTypeOf(value);
  LLVMTypeKind typeKind = LLVMGetTypeKind(valueType);

  // Check if it's a double type
  int isFloat = (typeKind == LLVMDoubleTypeKind) ? 1 : 0;

  // Return constant 1 or 0
  return LLVMConstInt(gen->intType, isFloat ? 1 : 0, 0);
}

/**
 * Compile is_string type guard
 *
 * Syntax: (is_string value)
 *
 * Behavior:
 * - Compiles value expression
 * - Checks if LLVM type is PointerType pointing to i8
 * - Returns constant 1 (true) or 0 (false)
 *
 * LLVM Implementation:
 * - Get type: LLVMTypeOf(value)
 * - Check kind: LLVMGetTypeKind(type) == LLVMPointerTypeKind
 * - Check element: LLVMGetElementType(type) is i8
 * - Return: LLVMConstInt(i64, match ? 1 : 0)
 *
 * Example:
 *   (is_string "hello")  → 1 (compile-time constant)
 *   (is_string 42)       → 0 (compile-time constant)
 *   (is_string 3.14)     → 0 (compile-time constant)
 */
LLVMValueRef LLVMCodeGen_compileIsString(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: is_string requires exactly 1 argument (value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile the value expression
  AstNode *valueNode = node->children[0];
  LLVMValueRef value = LLVMCodeGen_compileNode(gen, valueNode);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile is_string argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Get LLVM type of the value
  LLVMTypeRef valueType = LLVMTypeOf(value);
  LLVMTypeKind typeKind = LLVMGetTypeKind(valueType);

  // Check if it's a pointer type
  int isString = 0;
  if (typeKind == LLVMPointerTypeKind) {
    // In opaque pointer mode (LLVM 15+), we can't check element type reliably
    // We assume all pointers in Franz are strings for now
    // TODO: Add runtime type tagging for precise checks
    isString = 1;
  }

  // Return constant 1 or 0
  return LLVMConstInt(gen->intType, isString ? 1 : 0, 0);
}

/**
 * Compile is_list type guard
 *
 * Syntax: (is_list value)
 *
 * Behavior:
 * - Lists are not yet fully supported in LLVM mode
 * - Returns constant 0 (false) for now
 * - TODO: Implement list support with runtime type tags
 *
 * Future Implementation:
 * - Add TYPE_LIST tag to runtime values
 * - Check tag at compile time or runtime
 * - Return appropriate boolean result
 */
LLVMValueRef LLVMCodeGen_compileIsList(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: is_list requires exactly 1 argument (value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile the value expression
  AstNode *valueNode = node->children[0];
  LLVMValueRef value = LLVMCodeGen_compileNode(gen, valueNode);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile is_list argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  //  Optimize type checking at compile time
  LLVMTypeRef valueType = LLVMTypeOf(value);
  LLVMTypeKind typeKind = LLVMGetTypeKind(valueType);

  // If it's a native type (int, float), return 0 immediately
  if (typeKind == LLVMIntegerTypeKind || typeKind == LLVMDoubleTypeKind) {
    return LLVMConstInt(gen->intType, 0, 0);
  }

  // If it's a string literal, return 0
  if (typeKind == LLVMPointerTypeKind && valueNode->opcode == OP_STRING) {
    return LLVMConstInt(gen->intType, 0, 0);
  }

  // Otherwise call franz_is_list for pointer types (could be Generic*)
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Declare franz_is_list if not already declared
  LLVMValueRef isListFunc = LLVMGetNamedFunction(gen->module, "franz_is_list");
  if (!isListFunc) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    isListFunc = LLVMAddFunction(gen->module, "franz_is_list", funcType);
  }

  // Call franz_is_list(value)
  LLVMValueRef args[] = { value };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(isListFunc),
                        isListFunc, args, 1, "is_list");
}

/**
 * Compile is_function type guard
 *
 * Syntax: (is_function value)
 *
 * Behavior:
 * - Functions are not yet fully supported in LLVM mode
 * - Returns constant 0 (false) for now
 * - TODO: Implement function type checking
 *
 * Future Implementation:
 * - Check if value is LLVMFunctionTypeKind
 * - Or check if value is pointer to function
 * - Return appropriate boolean result
 */
LLVMValueRef LLVMCodeGen_compileIsFunction(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: is_function requires exactly 1 argument (value) at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile the value expression (for validation)
  AstNode *valueNode = node->children[0];
  LLVMValueRef value = LLVMCodeGen_compileNode(gen, valueNode);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile is_function argument at line %d\n", valueNode->lineNumber);
    return NULL;
  }

  // Functions not yet supported in LLVM mode - return constant 0
  fprintf(stderr, "WARNING: is_function not yet fully implemented in LLVM mode at line %d\n",
          node->lineNumber);
  return LLVMConstInt(gen->intType, 0, 0);
}
