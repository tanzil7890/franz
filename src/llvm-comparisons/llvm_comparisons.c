#include "llvm_comparisons.h"
#include "../llvm-closures/llvm_closures.h"
#include "../llvm-unboxing/llvm_unboxing.h"
#include <stdio.h>
#include <string.h>

/**
 *  Comparison Operators for LLVM Native Compilation
 *
 * This module implements three comparison operators:
 * 1. is         - Equality comparison (int, float, string)
 * 2. less_than  - Less-than comparison (int, float with type promotion)
 * 3. greater_than - Greater-than comparison (int, float with type promotion)
 *
 * All functions return TYPE_INT (0 or 1) representing boolean false/true.
 */

// Forward declaration for node compilation
extern LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);

// Helper function: Promote integer to float
static void promoteToFloat(LLVMCodeGen *gen, LLVMValueRef *value, LLVMTypeRef *type) {
  if (*type == gen->intType) {
    *value = LLVMBuildSIToFP(gen->builder, *value, gen->floatType, "itof");
    *type = gen->floatType;
  }
}

static LLVMValueRef LLVMCodeGen_boxParamValue(LLVMCodeGen *gen,
                                              LLVMValueRef rawValue,
                                              LLVMValueRef tagValue,
                                              const char *labelPrefix,
                                              int sequence) {
  (void)labelPrefix;
  (void)sequence;
  if (!tagValue) return NULL;

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef valueAsInt = rawValue;
  LLVMTypeRef rawType = LLVMTypeOf(rawValue);

  if (rawType == gen->floatType) {
    valueAsInt = LLVMBuildBitCast(gen->builder, rawValue, gen->intType, "param_box_float_bits");
  } else if (LLVMGetTypeKind(rawType) == LLVMPointerTypeKind) {
    valueAsInt = LLVMBuildPtrToInt(gen->builder, rawValue, gen->intType, "param_box_ptr_bits");
  } else if (rawType != gen->intType) {
    valueAsInt = LLVMBuildPtrToInt(gen->builder, rawValue, gen->intType, "param_box_bits");
  }

  LLVMValueRef boxFunc = LLVMGetNamedFunction(gen->module, "franz_box_param_tag");
  if (!boxFunc) {
    LLVMTypeRef params[] = {gen->intType, LLVMInt32TypeInContext(gen->context)};
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 2, 0);
    boxFunc = LLVMAddFunction(gen->module, "franz_box_param_tag", funcType);
  }

  LLVMValueRef args[] = {valueAsInt, tagValue};
  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFunc), boxFunc, args, 2, "param_box_call");
}

static LLVMValueRef LLVMCodeGen_buildVoidCondition(LLVMCodeGen *gen, AstNode *operand) {
  if (!operand) {
    return NULL;
  }

  LLVMTypeRef boolType = LLVMInt1TypeInContext(gen->context);

  if (operand->opcode == OP_IDENTIFIER && operand->val) {
    // BUGFIX: Distinguish between void literal and void as a captured free variable
    // When void is captured as a free variable, it's a runtime value, not compile-time constant
    int isVoidLiteral = (strcmp(operand->val, "void") == 0);

    // Check if this identifier is actually a variable in scope (free variable or parameter)
    LLVMValueRef varValue = gen->variables ? LLVMVariableMap_get(gen->variables, operand->val) : NULL;
    int isVariable = (varValue != NULL);

    // If "void" is a captured free variable, generate runtime check instead of constant
    if (isVoidLiteral && isVariable) {
      // Compare the captured void value with void sentinel (10)
      LLVMValueRef voidSentinel = LLVMConstInt(LLVMInt64TypeInContext(gen->context), 10, 0);
      return LLVMBuildICmp(gen->builder, LLVMIntEQ, varValue, voidSentinel, "void_var_check");
    }

    // Only return constant true for void literal if it's NOT a runtime variable
    if (isVoidLiteral && !isVariable) {
      return LLVMConstInt(boolType, 1, 0);
    }

    if (gen->voidVariables) {
      LLVMValueRef marker = LLVMVariableMap_get(gen->voidVariables, operand->val);
      if (marker) {
        return LLVMConstInt(boolType, 1, 0);
      }
    }

    if (gen->paramTypeTags) {
      LLVMValueRef tagValue = LLVMVariableMap_get(gen->paramTypeTags, operand->val);
      if (tagValue) {
        LLVMValueRef voidTag = LLVMConstInt(LLVMInt32TypeInContext(gen->context),
                                            CLOSURE_RETURN_VOID, 0);
        return LLVMBuildICmp(gen->builder, LLVMIntEQ, tagValue, voidTag, "param_is_void");
      }
    }
  }

  return NULL;
}

static LLVMValueRef LLVMCodeGen_buildIsStandardComparison(LLVMCodeGen *gen,
                                                          AstNode *node,
                                                          LLVMValueRef left,
                                                          LLVMValueRef right) {
  LLVMTypeRef leftType = LLVMTypeOf(left);
  LLVMTypeRef rightType = LLVMTypeOf(right);

  fprintf(stderr, "[IS DEBUG] Comparing types: left kind=%d, right kind=%d\n",
          LLVMGetTypeKind(leftType), LLVMGetTypeKind(rightType));
  fprintf(stderr, "[IS DEBUG] leftType == intType: %d, rightType == intType: %d\n",
          leftType == gen->intType, rightType == gen->intType);
  fprintf(stderr, "[IS DEBUG] leftType == stringType: %d, rightType == stringType: %d\n",
          leftType == gen->stringType, rightType == gen->stringType);

  extern int isGenericPointerNode(LLVMCodeGen *gen, AstNode *node);

  int leftIsGeneric = isGenericPointerNode(gen, node->children[0]);
  int rightIsGeneric = isGenericPointerNode(gen, node->children[1]);

  fprintf(stderr, "[IS DEBUG] leftIsGeneric=%d, rightIsGeneric=%d\n", leftIsGeneric, rightIsGeneric);

  if (leftIsGeneric || rightIsGeneric) {
    fprintf(stderr, "[IS DEBUG] Taking Generic* comparison path\n");

    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

    LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
    if (!boxStringFunc) {
      LLVMTypeRef params[] = { gen->stringType };
      LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
      boxStringFunc = LLVMAddFunction(gen->module, "franz_box_string", funcType);
    }

    LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
    if (!boxIntFunc) {
      LLVMTypeRef params[] = { gen->intType };
      LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
      boxIntFunc = LLVMAddFunction(gen->module, "franz_box_int", funcType);
    }

    LLVMValueRef boxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_box_float");
    if (!boxFloatFunc) {
      LLVMTypeRef params[] = { gen->floatType };
      LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
      boxFloatFunc = LLVMAddFunction(gen->module, "franz_box_float", funcType);
    }

    // FIX: Check if operands are closure parameters (use runtime tags)
    LLVMValueRef leftParamTag = NULL;
    LLVMValueRef rightParamTag = NULL;
    LLVMValueRef leftParamBoxed = NULL;
    LLVMValueRef rightParamBoxed = NULL;

    if (gen->paramTypeTags) {
      if (node->children[0]->opcode == OP_IDENTIFIER && node->children[0]->val) {
        leftParamTag = LLVMVariableMap_get(gen->paramTypeTags, node->children[0]->val);
        if (leftParamTag) {
          leftParamBoxed = LLVMCodeGen_boxParamValue(gen, left, leftParamTag, "is_left_param", 0);
          leftIsGeneric = 0;  // runtime tag drives boxing
        }
      }
      if (node->children[1]->opcode == OP_IDENTIFIER && node->children[1]->val) {
        rightParamTag = LLVMVariableMap_get(gen->paramTypeTags, node->children[1]->val);
        if (rightParamTag) {
          rightParamBoxed = LLVMCodeGen_boxParamValue(gen, right, rightParamTag, "is_right_param", 0);
          rightIsGeneric = 0;
        }
      }
    }

    int needsGeneric = leftIsGeneric || rightIsGeneric || (leftParamBoxed != NULL) || (rightParamBoxed != NULL);

    LLVMValueRef leftPtr = left;
    LLVMValueRef rightPtr = right;

    if (needsGeneric) {
      if (leftParamBoxed) {
        leftPtr = leftParamBoxed;
      } else if (leftIsGeneric) {
        if (leftType == gen->intType) {
          fprintf(stderr, "[IS DEBUG] Left tracked as Generic*, converting i64→ptr\n");
          leftPtr = LLVMBuildIntToPtr(gen->builder, left, genericPtrType, "left_tracked_generic_ptr");
        } else {
          fprintf(stderr, "[IS DEBUG] Left already Generic* pointer\n");
          leftPtr = left;
        }
      } else if (leftType == gen->intType) {
        fprintf(stderr, "[IS DEBUG] Boxing left integer into Generic*\n");
        LLVMValueRef args[] = { left };
        leftPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                 boxIntFunc, args, 1, "left_boxed_int");
      } else if (leftType == gen->floatType) {
        fprintf(stderr, "[IS DEBUG] Boxing left float into Generic*\n");
        LLVMValueRef args[] = { left };
        leftPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc),
                                 boxFloatFunc, args, 1, "left_boxed_float");
      } else if (leftType == gen->stringType) {
        fprintf(stderr, "[IS DEBUG] Boxing left raw string into Generic*\n");
        LLVMValueRef args[] = { left };
        leftPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                 boxStringFunc, args, 1, "left_boxed");
      }

      if (rightParamBoxed) {
        rightPtr = rightParamBoxed;
      } else if (rightIsGeneric) {
        if (rightType == gen->intType) {
          fprintf(stderr, "[IS DEBUG] Right tracked as Generic*, converting i64→ptr\n");
          rightPtr = LLVMBuildIntToPtr(gen->builder, right, genericPtrType, "right_tracked_generic_ptr");
        } else {
          fprintf(stderr, "[IS DEBUG] Right is already Generic* pointer\n");
          rightPtr = right;
        }
      } else if (rightType == gen->intType) {
        fprintf(stderr, "[IS DEBUG] Boxing right integer into Generic*\n");
        LLVMValueRef args[] = { right };
        rightPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                  boxIntFunc, args, 1, "right_boxed_int");
      } else if (rightType == gen->floatType) {
        fprintf(stderr, "[IS DEBUG] Boxing right float into Generic*\n");
        LLVMValueRef args[] = { right };
        rightPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc),
                                  boxFloatFunc, args, 1, "right_boxed_float");
      } else if (rightType == gen->stringType) {
        fprintf(stderr, "[IS DEBUG] Boxing right raw string into Generic*\n");
        LLVMValueRef args[] = { right };
        rightPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                  boxStringFunc, args, 1, "right_boxed");
      }

      LLVMValueRef franzIsFunc = LLVMGetNamedFunction(gen->module, "franz_generic_is");
      if (!franzIsFunc) {
        LLVMTypeRef params[] = { genericPtrType, genericPtrType };
        LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 2, 0);
        franzIsFunc = LLVMAddFunction(gen->module, "franz_generic_is", funcType);
      }

      fprintf(stderr, "[IS DEBUG] Calling franz_generic_is\n");
      LLVMValueRef args[] = { leftPtr, rightPtr };
      return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(franzIsFunc),
                            franzIsFunc, args, 2, "generic_is_result");
    }
  }

  if (leftType == gen->stringType && rightType == gen->stringType) {
    LLVMValueRef args[] = {left, right};
    LLVMValueRef cmpResult = LLVMBuildCall2(gen->builder, gen->strcmpType, gen->strcmpFunc,
                                             args, 2, "strcmp");

    LLVMValueRef zero = LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0);
    LLVMValueRef cmp = LLVMBuildICmp(gen->builder, LLVMIntEQ, cmpResult, zero, "streq");

    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "streq_result");
  }

  left = LLVMUnboxing_unboxForArithmetic(gen, left, node->children[0]);
  right = LLVMUnboxing_unboxForArithmetic(gen, right, node->children[1]);

  leftType = LLVMTypeOf(left);
  rightType = LLVMTypeOf(right);

  if (leftType == gen->intType && rightType == gen->intType) {
    LLVMValueRef cmp = LLVMBuildICmp(gen->builder, LLVMIntEQ, left, right, "ieq");
    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "ieq_result");
  }

  if (leftType == gen->floatType || rightType == gen->floatType) {
    if (leftType == gen->intType) {
      left = LLVMBuildSIToFP(gen->builder, left, gen->floatType, "itof_left");
      leftType = gen->floatType;
    }
    if (rightType == gen->intType) {
      right = LLVMBuildSIToFP(gen->builder, right, gen->floatType, "itof_right");
      rightType = gen->floatType;
    }

    LLVMValueRef cmp = LLVMBuildFCmp(gen->builder, LLVMRealOEQ, left, right, "feq");
    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "feq_result");
  }

  fprintf(stderr, "ERROR: is comparison not supported for these types at line %d\n",
          node->lineNumber);
  return NULL;
}

/**
 * Compile equality comparison (is)
 *
 * Syntax: (is a b)
 *
 * Supports:
 * - Integer equality: (is 5 5) → 1
 * - Float equality: (is 3.14 3.14) → 1
 * - String equality: (is "hello" "hello") → 1
 * - Mixed int/float: (is 5 5.0) → 1 (with type promotion)
 *
 * Returns: i64 (0 or 1)
 *
 * Implementation:
 * - Integers: Uses LLVM icmp eq
 * - Floats: Uses LLVM fcmp oeq (ordered equal, NaN ≠ NaN)
 * - Strings: Calls C strcmp() via LLVM
 * - Result: zext i1 to i64 (convert bool to integer)
 */
LLVMValueRef LLVMCodeGen_compileIs(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: is requires exactly 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef left = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef right = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!left || !right) {
    return NULL;
  }

  LLVMValueRef leftVoidCond = LLVMCodeGen_buildVoidCondition(gen, node->children[0]);
  LLVMValueRef rightVoidCond = LLVMCodeGen_buildVoidCondition(gen, node->children[1]);

  if (!leftVoidCond && !rightVoidCond) {
    return LLVMCodeGen_buildIsStandardComparison(gen, node, left, right);
  }

  LLVMTypeRef boolType = LLVMInt1TypeInContext(gen->context);
  if (!leftVoidCond) {
    leftVoidCond = LLVMConstInt(boolType, 0, 0);
  }
  if (!rightVoidCond) {
    rightVoidCond = LLVMConstInt(boolType, 0, 0);
  }

  LLVMValueRef eitherVoid = LLVMBuildOr(gen->builder, leftVoidCond, rightVoidCond, "either_void");

  LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
  LLVMValueRef currentFunction = LLVMGetBasicBlockParent(currentBlock);
  LLVMBasicBlockRef voidBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunction, "is_void_case");
  LLVMBasicBlockRef nonVoidBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunction, "is_nonvoid_case");
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunction, "is_merge");

  LLVMBuildCondBr(gen->builder, eitherVoid, voidBlock, nonVoidBlock);

  LLVMPositionBuilderAtEnd(gen->builder, voidBlock);
  LLVMValueRef bothVoid = LLVMBuildAnd(gen->builder, leftVoidCond, rightVoidCond, "both_void");
  LLVMValueRef voidResult = LLVMBuildZExt(gen->builder, bothVoid, gen->intType, "void_case_result");
  LLVMBuildBr(gen->builder, mergeBlock);

  LLVMPositionBuilderAtEnd(gen->builder, nonVoidBlock);
  LLVMValueRef nonVoidResult = LLVMCodeGen_buildIsStandardComparison(gen, node, left, right);
  if (!nonVoidResult) {
    return NULL;
  }
  LLVMBuildBr(gen->builder, mergeBlock);

  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, gen->intType, "is_result");
  LLVMValueRef incomingValues[2] = {voidResult, nonVoidResult};
  LLVMBasicBlockRef incomingBlocks[2] = {voidBlock, nonVoidBlock};
  LLVMAddIncoming(phi, incomingValues, incomingBlocks, 2);
  return phi;
}

/**
 * Compile less-than comparison
 *
 * Syntax: (less_than a b)
 *
 * Supports:
 * - Integer comparison: (less_than 3 5) → 1
 * - Float comparison: (less_than 3.5 5.2) → 1
 * - Mixed int/float: (less_than 3 5.5) → 1 (with type promotion)
 *
 * Returns: i64 (0 or 1)
 *
 * Implementation:
 * - Integers: Uses LLVM icmp slt (signed less than)
 * - Floats: Uses LLVM fcmp olt (ordered less than)
 * - Automatic type promotion for mixed types
 */
LLVMValueRef LLVMCodeGen_compileLessThan(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: less_than requires exactly 2 arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile both operands
  LLVMValueRef left = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef right = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!left || !right) {
    return NULL;
  }

  // CRITICAL: Unbox i64 parameters (Generic* cast to i64) before comparing
  left = LLVMUnboxing_unboxForArithmetic(gen, left, node->children[0]);
  right = LLVMUnboxing_unboxForArithmetic(gen, right, node->children[1]);

  LLVMTypeRef leftType = LLVMTypeOf(left);
  LLVMTypeRef rightType = LLVMTypeOf(right);

  // Case 1: Both integers
  if (leftType == gen->intType && rightType == gen->intType) {
    // Integer less-than: icmp slt (signed less than)
    LLVMValueRef cmp = LLVMBuildICmp(gen->builder, LLVMIntSLT, left, right, "ilt");
    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "ilt_result");
  }

  // Case 2: Both floats or mixed (promote to float)
  if (leftType == gen->floatType || rightType == gen->floatType) {
    // Promote integers to float
    promoteToFloat(gen, &left, &leftType);
    promoteToFloat(gen, &right, &rightType);

    // Float less-than: fcmp olt (ordered less than)
    LLVMValueRef cmp = LLVMBuildFCmp(gen->builder, LLVMRealOLT, left, right, "flt");
    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "flt_result");
  }

  // Error: unsupported types
  fprintf(stderr, "ERROR: less_than comparison requires numeric types at line %d\n",
          node->lineNumber);
  return NULL;
}

/**
 * Compile greater-than comparison
 *
 * Syntax: (greater_than a b)
 *
 * Supports:
 * - Integer comparison: (greater_than 10 3) → 1
 * - Float comparison: (greater_than 7.5 3.2) → 1
 * - Mixed int/float: (greater_than 10 3.5) → 1 (with type promotion)
 *
 * Returns: i64 (0 or 1)
 *
 * Implementation:
 * - Integers: Uses LLVM icmp sgt (signed greater than)
 * - Floats: Uses LLVM fcmp ogt (ordered greater than)
 * - Automatic type promotion for mixed types
 */
LLVMValueRef LLVMCodeGen_compileGreaterThan(LLVMCodeGen *gen, AstNode *node) {
  // Validate argument count
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: greater_than requires exactly 2 arguments at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile both operands
  LLVMValueRef left = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef right = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!left || !right) {
    return NULL;
  }

  // CRITICAL: Unbox i64 parameters (Generic* cast to i64) before comparing
  // This is needed when closures receive parameters from runtime
  left = LLVMUnboxing_unboxForArithmetic(gen, left, node->children[0]);
  right = LLVMUnboxing_unboxForArithmetic(gen, right, node->children[1]);

  LLVMTypeRef leftType = LLVMTypeOf(left);
  LLVMTypeRef rightType = LLVMTypeOf(right);

  // Case 1: Both integers
  if (leftType == gen->intType && rightType == gen->intType) {
    // Integer greater-than: icmp sgt (signed greater than)
    LLVMValueRef cmp = LLVMBuildICmp(gen->builder, LLVMIntSGT, left, right, "igt");
    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "igt_result");
  }

  // Case 2: Both floats or mixed (promote to float)
  if (leftType == gen->floatType || rightType == gen->floatType) {
    // Promote integers to float
    promoteToFloat(gen, &left, &leftType);
    promoteToFloat(gen, &right, &rightType);

    // Float greater-than: fcmp ogt (ordered greater than)
    LLVMValueRef cmp = LLVMBuildFCmp(gen->builder, LLVMRealOGT, left, right, "fgt");
    return LLVMBuildZExt(gen->builder, cmp, gen->intType, "fgt_result");
  }

  // Error: unsupported types
  fprintf(stderr, "ERROR: greater_than comparison requires numeric types at line %d\n",
          node->lineNumber);
  return NULL;
}
#include <string.h>
