#include "llvm_lists.h"
#include "../stdlib.h"
#include <stdio.h>

/**
 *  Full LLVM List Literal Compilation
 *
 * Strategy:
 * 1. Compile each child element to native LLVM value (i64, double, i8*)
 * 2. Box each value into Generic* using runtime helpers
 * 3. Store Generic* pointers in an array (alloca)
 * 4. Call franz_list_new(Generic**, int) runtime function
 * 5. Return opaque pointer (i8*) to list Generic*
 */
LLVMValueRef LLVMLists_compileList(LLVMCodeGen *gen, AstNode *node) {
  // Static counter for generating unique list variable names
  // This prevents PHI node conflicts when lists are created in if-else branches
  static int listCounter = 0;

  if (!gen || !node) {
    fprintf(stderr, "ERROR: Invalid parameters to LLVMLists_compileList\n");
    return NULL;
  }

  int elementCount = node->childCount;

  // Declare runtime boxing functions if not already declared
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_box_int(i64) -> Generic*
  LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
  if (!boxIntFunc) {
    LLVMTypeRef boxIntParams[] = { gen->intType };
    LLVMTypeRef boxIntType = LLVMFunctionType(genericPtrType, boxIntParams, 1, 0);
    boxIntFunc = LLVMAddFunction(gen->module, "franz_box_int", boxIntType);
  }

  // franz_box_float(double) -> Generic*
  LLVMValueRef boxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_box_float");
  if (!boxFloatFunc) {
    LLVMTypeRef boxFloatParams[] = { gen->floatType };
    LLVMTypeRef boxFloatType = LLVMFunctionType(genericPtrType, boxFloatParams, 1, 0);
    boxFloatFunc = LLVMAddFunction(gen->module, "franz_box_float", boxFloatType);
  }

  // franz_box_string(i8*) -> Generic*
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
  if (!boxStringFunc) {
    LLVMTypeRef boxStringParams[] = { gen->stringType };
    LLVMTypeRef boxStringType = LLVMFunctionType(genericPtrType, boxStringParams, 1, 0);
    boxStringFunc = LLVMAddFunction(gen->module, "franz_box_string", boxStringType);
  }

  // franz_box_list(Generic*) -> Generic*
  LLVMValueRef boxListFunc = LLVMGetNamedFunction(gen->module, "franz_box_list");
  if (!boxListFunc) {
    LLVMTypeRef boxListParams[] = { genericPtrType };
    LLVMTypeRef boxListType = LLVMFunctionType(genericPtrType, boxListParams, 1, 0);
    boxListFunc = LLVMAddFunction(gen->module, "franz_box_list", boxListType);
  }

  // franz_list_new(Generic**, int) -> Generic*
  LLVMValueRef listNewFunc = LLVMGetNamedFunction(gen->module, "franz_list_new");
  if (!listNewFunc) {
    LLVMTypeRef genericPtrPtrType = LLVMPointerType(genericPtrType, 0);
    LLVMTypeRef listNewParams[] = { genericPtrPtrType, gen->intType };
    LLVMTypeRef listNewType = LLVMFunctionType(genericPtrType, listNewParams, 2, 0);
    listNewFunc = LLVMAddFunction(gen->module, "franz_list_new", listNewType);
  }

  // Empty list: []
  if (elementCount == 0) {
    // Call franz_list_new(NULL, 0)
    LLVMTypeRef genericPtrPtrType = LLVMPointerType(genericPtrType, 0);
    LLVMValueRef nullPtr = LLVMConstPointerNull(genericPtrPtrType);
    LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
    LLVMValueRef args[] = { nullPtr, zero };

    // Generate unique name to avoid PHI node conflicts
    char listName[32];
    snprintf(listName, sizeof(listName), "list_%d", listCounter++);

    return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNewFunc),
                          listNewFunc, args, 2, listName);
  }

  // Non-empty list: [elem1, elem2, ...]
  // Allocate array for Generic* pointers
  LLVMValueRef arraySize = LLVMConstInt(gen->intType, elementCount, 0);
  LLVMValueRef array = LLVMBuildArrayAlloca(gen->builder, genericPtrType, arraySize, "list_array");

  // Compile and box each element
  for (int i = 0; i < elementCount; i++) {
    // Compile child element
    LLVMValueRef elemValue = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!elemValue) {
      fprintf(stderr, "ERROR: Failed to compile list element %d at line %d\n",
              i, node->lineNumber);
      return NULL;
    }

    // Determine element type and box it
    LLVMTypeRef elemType = LLVMTypeOf(elemValue);
    LLVMTypeKind typeKind = LLVMGetTypeKind(elemType);

    LLVMValueRef boxedElem = NULL;

    // CRITICAL FIX Issue #2: Check if element is already a Generic* pointer
    // Variables tracked in genericVariables are closure results stored as i64 (Generic* cast to i64)
    // These should be converted back to pointer, NOT boxed as raw integers!
    int isAlreadyGeneric = 0;
    if (node->children[i]->opcode == OP_IDENTIFIER && node->children[i]->val) {
      if (LLVMVariableMap_get(gen->genericVariables, node->children[i]->val) != NULL) {
        isAlreadyGeneric = 1;
      }
    }

    if (typeKind == LLVMIntegerTypeKind) {
      if (isAlreadyGeneric) {
        // This i64 is actually a Generic* pointer cast to i64 - convert back to pointer
        boxedElem = LLVMBuildIntToPtr(gen->builder, elemValue, genericPtrType, "generic_ptr");
      } else {
        // Raw integer: box with franz_box_int
        LLVMValueRef boxArgs[] = { elemValue };
        boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                    boxIntFunc, boxArgs, 1, "boxed_int");
      }
    } else if (typeKind == LLVMDoubleTypeKind) {
      // Float: box with franz_box_float
      LLVMValueRef boxArgs[] = { elemValue };
      boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc),
                                  boxFloatFunc, boxArgs, 1, "boxed_float");
    } else if (typeKind == LLVMPointerTypeKind) {
      // Could be string (i8*) or nested list (Generic*)
      // Check if it's already a Generic* (from nested list or closure)
      // For now, assume i8* is either string or Generic*

      // If element is OP_STRING, it's a string literal
      if (node->children[i]->opcode == OP_STRING) {
        LLVMValueRef boxArgs[] = { elemValue };
        boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                    boxStringFunc, boxArgs, 1, "boxed_string");
      } else if (node->children[i]->opcode == OP_LIST) {
        // Nested list - already returns Generic*
        LLVMValueRef boxArgs[] = { elemValue };
        boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxListFunc),
                                    boxListFunc, boxArgs, 1, "boxed_list");
      } else {
        // Other pointer types - treat as string for now
        LLVMValueRef boxArgs[] = { elemValue };
        boxedElem = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                    boxStringFunc, boxArgs, 1, "boxed_ptr");
      }
    } else {
      fprintf(stderr, "ERROR: Unsupported element type in list at line %d\n",
              node->lineNumber);
      return NULL;
    }

    // Store boxed element in array
    LLVMValueRef indices[] = { LLVMConstInt(gen->intType, i, 0) };
    LLVMValueRef elemPtr = LLVMBuildGEP2(gen->builder, genericPtrType, array,
                                          indices, 1, "elem_ptr");
    LLVMBuildStore(gen->builder, boxedElem, elemPtr);
  }

  // Call franz_list_new(array, elementCount)
  LLVMValueRef lengthVal = LLVMConstInt(gen->intType, elementCount, 0);
  LLVMValueRef args[] = { array, lengthVal };

  // Generate unique name to avoid PHI node conflicts in if-else blocks
  char listName[32];
  snprintf(listName, sizeof(listName), "list_%d", listCounter++);

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNewFunc),
                        listNewFunc, args, 2, listName);
}
