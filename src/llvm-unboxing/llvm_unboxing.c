#include "llvm_unboxing.h"
#include "../stdlib.h"
#include <stdio.h>
#include <string.h>

/**
 *  Auto-Unboxing Implementation
 *
 * This provides automatic conversion of Generic* pointers to native values
 * when list operations return boxed values that need to be used in arithmetic
 * or other operations expecting native types.
 */

// Helper: Check if AST node represents a list operation that returns Generic*
//  Now also checks if variable holds Generic* using tracking map
static int isListOperationNode(LLVMCodeGen *gen, AstNode *node) {
  if (!node) return 0;

  if (node->opcode == OP_APPLICATION && node->childCount > 0) {
    AstNode *funcNode = node->children[0];
    if (funcNode->opcode == OP_IDENTIFIER) {
      const char *name = funcNode->val;
      if (strcmp(name, "head") == 0 || strcmp(name, "tail") == 0 ||
          strcmp(name, "nth") == 0) {
        return 1;
      }
    }
  }

  //  Check if variable holds Generic* pointer
  if (node->opcode == OP_IDENTIFIER && node->val) {
    if (LLVMVariableMap_get(gen->genericVariables, node->val) != NULL) {
      return 1;  // Variable was assigned from list/list operation
    }
  }

  return 0;
}

int LLVMUnboxing_needsUnboxing(LLVMCodeGen *gen, LLVMValueRef value, AstNode *node) {
  // Check if the value is a pointer type AND comes from a list operation
  LLVMTypeRef valueType = LLVMTypeOf(value);
  if (LLVMGetTypeKind(valueType) != LLVMPointerTypeKind) {
    return 0;  // Not a pointer, no unboxing needed
  }

  // Check if the AST node is a list operation or generic variable
  return isListOperationNode(gen, node);
}

LLVMValueRef LLVMUnboxing_autoUnbox(LLVMCodeGen *gen, LLVMValueRef genericPtr,
                                     AstNode *node, LLVMTypeRef expectedType) {
  // Declare runtime unboxing functions if not already declared
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_unbox_int(Generic*) -> i64
  LLVMValueRef unboxIntFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_int");
  if (!unboxIntFunc) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    unboxIntFunc = LLVMAddFunction(gen->module, "franz_unbox_int", funcType);
  }

  // franz_unbox_float(Generic*) -> double
  LLVMValueRef unboxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_float");
  if (!unboxFloatFunc) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->floatType, params, 1, 0);
    unboxFloatFunc = LLVMAddFunction(gen->module, "franz_unbox_float", funcType);
  }

  // Determine which unboxing function to call based on expected type
  LLVMValueRef unboxFunc;
  if (expectedType == gen->intType) {
    unboxFunc = unboxIntFunc;
  } else if (expectedType == gen->floatType) {
    unboxFunc = unboxFloatFunc;
  } else {
    // For other types (string, list), return the Generic* as-is
    return genericPtr;
  }

  // Call the unboxing function
  LLVMValueRef args[] = { genericPtr };
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxFunc),
                        unboxFunc, args, 1, "unboxed");
}

LLVMValueRef LLVMUnboxing_unboxForArithmetic(LLVMCodeGen *gen, LLVMValueRef value, AstNode *node) {
  // Check if the value is a pointer type or i64 (which may represent a Generic* cast to i64)
  LLVMTypeRef valueType = LLVMTypeOf(value);

  if (LLVMGetTypeKind(valueType) == LLVMPointerTypeKind) {
    // It's a pointer, might be a Generic* from a list operation or variable
    // Try to unbox as integer
    return LLVMUnboxing_autoUnbox(gen, value, node, gen->intType);
  } else if (LLVMGetTypeKind(valueType) == LLVMIntegerTypeKind && LLVMTypeOf(value) == gen->intType) {
    // CRITICAL DISTINCTION (from LLVM IR boxing/unboxing documentation):
    // - Boxed values: passed as pointers (need unboxing)
    // - Unboxed values: passed as raw scalars (no unboxing needed)
    //
    // i64 values can be EITHER:
    // 1. Generic* pointers cast to i64 (need unboxing) - from runtime variables
    // 2. Raw integer values (no unboxing) - from literals, closure params, arithmetic
    //
    // Solution: Check if it's a constant OR a function parameter (closure params are raw i64)
    if (LLVMIsConstant(value)) {
      // Literal constant (e.g., "2") - already unboxed, use as-is
      return value;
    }

    // CLOSURE FIX: Check if this is a function parameter
    //  Function parameters CAN be Generic* if tracked in genericVariables
    // Only skip unboxing if it's NOT a Generic* parameter
    if (LLVMIsAArgument(value)) {
      // Check if this parameter is tracked as Generic* (closure params that might receive Generic*)
      if (node && node->opcode == OP_IDENTIFIER && node->val) {
        if (LLVMVariableMap_get(gen->genericVariables, node->val) != NULL) {
          // This parameter IS tracked as Generic* - proceed to unbox
          fprintf(stderr, "[UNBOX] Parameter '%s' is Generic* - will unbox\n", node->val);
        } else {
          // Not tracked as Generic* - it's a raw i64 parameter, don't unbox
          return value;
        }
      } else {
        // Can't determine - assume raw i64, don't unbox
        return value;
      }
    }

    // Non-constant i64 (variable or Generic* parameter) - may be Generic* pointer cast to i64
    //  Use semantic check for Generic* values (lists, closures, etc.)
    // Only unbox if it comes from a Generic* operation (list, closure, tracked variable)
    if (!isGenericPointerNode(gen, node)) {
      // Not from a Generic* operation - assume it's a raw integer from arithmetic
      return value;
    }

    // Convert i64 -> Generic* pointer, then unbox
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    LLVMValueRef asPtr = LLVMBuildIntToPtr(gen->builder, value, genericPtrType, "i64_to_generic_ptr");
    return LLVMUnboxing_autoUnbox(gen, asPtr, node, gen->intType);
  }

  return value;  // Not a pointer or i64, return as-is
}
