#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llvm_closures.h"
#include "../llvm-codegen/llvm_codegen.h"
#include "../freevar/freevar.h"
#include "../type-inference/type_infer.h"

static int LLVMClosures_nodeIsVoid(LLVMCodeGen *gen, AstNode *node) {
  if (!node) {
    return 0;
  }

  if (node->opcode == OP_IDENTIFIER && node->val) {
    if (strcmp(node->val, "void") == 0) {
      return 1;
    }

    if (gen->voidVariables) {
      LLVMValueRef marker = LLVMVariableMap_get(gen->voidVariables, node->val);
      if (marker) {
        return 1;
      }
    }
  }

  return 0;
}

static void LLVMClosures_restoreParamTagState(LLVMCodeGen *gen,
                                              LLVMVariableMap *savedMap,
                                              LLVMVariableMap *currentMap) {
  if (currentMap) {
    LLVMVariableMap_free(currentMap);
  }
  gen->paramTypeTags = savedMap;
}

//  LLVM Closure Implementation
// Complete industry-standard closure support with lexical scope capture

// ============================================================================
// Helper: Check if function is a closure
// ============================================================================

int LLVMClosures_isClosure(AstNode *node) {
  if (!node || node->opcode != OP_FUNCTION) {
    return 0;
  }

  // Ensure free variable analysis has been done
  if (node->freeVarsCount == 0) {
    FreeVar_analyze(node);
  }

  // A function is a closure if it has free variables
  return (node->freeVarsCount > 0);
}

// ============================================================================
// Closure Type Definition
// ============================================================================

// Helper: Determine return type tag from LLVM type (PUBLIC - used by llvm_ir_gen.c)
int LLVMClosures_getReturnTypeTag(LLVMCodeGen *gen, LLVMTypeRef returnType) {
  LLVMTypeKind kind = LLVMGetTypeKind(returnType);

  // INDUSTRY-STANDARD FIX: Correctly map LLVM types to Franz type tags
  // The wrapper converts values to i8* (universal return type), but we need to track
  // the ORIGINAL type so we can convert back correctly in the caller
  //
  // - i64 → INT tag (wrapper does inttoptr, caller does ptrtoint to reverse)
  // - i8* → POINTER tag (wrapper does pointer cast, caller keeps as pointer)
  // - double → FLOAT tag (wrapper does bitcast+inttoptr, caller reverses)

  if (kind == LLVMIntegerTypeKind) {
    return CLOSURE_RETURN_INT;  // i64 → INT (raw integers like 42)
  } else if (kind == LLVMDoubleTypeKind || kind == LLVMFloatTypeKind) {
    return CLOSURE_RETURN_FLOAT;  // double/float
  } else {
    return CLOSURE_RETURN_POINTER;  // i8* (string, closure, list, etc.)
  }
}

LLVMTypeRef LLVMClosures_getClosureType(LLVMContextRef context) {
  //  Enhanced closure struct for polymorphic identity functions
  // Like Rust's dyn Trait or C++'s std::function
  // Struct: { i8*, i8*, i32, i32 } = { function_ptr, env_ptr, return_type_tag, param_index }
  LLVMTypeRef fields[4];
  fields[0] = LLVMPointerType(LLVMInt8TypeInContext(context), 0);  // function pointer (type-erased)
  fields[1] = LLVMPointerType(LLVMInt8TypeInContext(context), 0);  // environment pointer
  fields[2] = LLVMInt32TypeInContext(context);                      // return type tag (0=int, 1=float, 2=ptr, 5=dynamic)
  fields[3] = LLVMInt32TypeInContext(context);                      // parameter index (for DYNAMIC tag, which param to use)

  return LLVMStructTypeInContext(context, fields, 4, 0);  // 0 = not packed
}

// ============================================================================
// Environment Type Creation
// ============================================================================

LLVMTypeRef LLVMClosures_createEnvType(LLVMCodeGen *gen, char **freeVars, int freeVarsCount) {
  if (freeVarsCount == 0) {
    // No captured variables - return empty struct
    return LLVMStructTypeInContext(gen->context, NULL, 0, 0);
  }

  // CRITICAL FIX: Count only variables that need capturing
  // Skip variables that are in gen->functions - they remain accessible
  int actualFieldCount = 0;
  for (int i = 0; i < freeVarsCount; i++) {
    // Skip if variable is in functions map (still accessible in closure)
    if (LLVMVariableMap_get(gen->functions, freeVars[i])) {
      continue;
    }
    actualFieldCount++;
  }

  if (actualFieldCount == 0) {
    // All free vars are functions - no need for environment
    return LLVMStructTypeInContext(gen->context, NULL, 0, 0);
  }

  // Create struct with one field per captured variable (excluding functions)
  LLVMTypeRef *fields = malloc(actualFieldCount * sizeof(LLVMTypeRef));

  int fieldIdx = 0;
  for (int i = 0; i < freeVarsCount; i++) {
    // Skip if variable is in functions map
    if (LLVMVariableMap_get(gen->functions, freeVars[i])) {
      continue;
    }

    // Look up the type of each captured variable
    LLVMValueRef varValue = LLVMVariableMap_get(gen->variables, freeVars[i]);

    if (varValue) {
      // Capture-by-value: if the variable is an alloca (stack slot), use its allocated type
      // Otherwise use the value's type directly.
      if (LLVMIsAAllocaInst(varValue)) {
        fields[fieldIdx] = LLVMGetAllocatedType(varValue);
      } else {
        // Use actual type of the value
        fields[fieldIdx] = LLVMTypeOf(varValue);
      }
    } else {
      // Fallback to i64 (most common type in Franz)
      fields[fieldIdx] = gen->intType;
    }
    fieldIdx++;
  }

  LLVMTypeRef envType = LLVMStructTypeInContext(gen->context, fields, actualFieldCount, 0);
  free(fields);

  return envType;
}

// ============================================================================
// Closure Compilation
// ============================================================================

LLVMValueRef LLVMClosures_compileClosure(LLVMCodeGen *gen, AstNode *node) {
  if (!node || node->opcode != OP_FUNCTION) {
    fprintf(stderr, "ERROR: LLVMClosures_compileClosure called on non-function node\n");
    return NULL;
  }

  // Ensure free variable analysis has been done (re-run if empty)
  if (node->freeVarsCount == 0) {
    FreeVar_analyze(node);
  }

  int freeVarsCount = node->freeVarsCount;
  char **freeVars = node->freeVars;

  LLVMVariableMap *savedParamTagMap = gen->paramTypeTags;
  LLVMVariableMap *closureParamTagMap = LLVMVariableMap_new();
  gen->paramTypeTags = closureParamTagMap;

#define RESTORE_PARAM_TAGS_AND_RETURN(value)                     \
  do {                                                           \
    LLVMClosures_restoreParamTagState(gen, savedParamTagMap,     \
                                      closureParamTagMap);       \
    return (value);                                              \
  } while (0)

  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE COMPILE] Free vars count = %d\n", freeVarsCount);
  #endif
  for (int i = 0; i < freeVarsCount; i++) {
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE]   Free var %d: %s\n", i, freeVars[i]);
  #endif
  }

  // Step 1: Create environment struct type
  LLVMTypeRef envType = LLVMClosures_createEnvType(gen, freeVars, freeVarsCount);

  // Step 2: Allocate environment on heap
  LLVMValueRef envPtr = LLVMBuildMalloc(gen->builder, envType, "closure_env");

  // Step 3: Store captured variables into environment
  // CRITICAL FIX: Skip variables in gen->functions - they remain accessible
  int fieldIdx = 0;
  for (int i = 0; i < freeVarsCount; i++) {
    // Skip if variable is in functions map (still accessible in closure)
    if (LLVMVariableMap_get(gen->functions, freeVars[i])) {
      // Functions in gen->functions remain accessible during closure execution
      // No need to capture them - they're global to the closure context
      continue;
    }

    // Look up captured variable's current value
    LLVMValueRef varValue = LLVMVariableMap_get(gen->variables, freeVars[i]);

    if (!varValue) {
      fprintf(stderr, "WARNING: Free variable '%s' not found in current scope, skipping\n", freeVars[i]);
      continue;
    }

    // Get pointer to field in environment struct (using fieldIdx, not i)
    LLVMValueRef indices[2] = {
      LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),  // Struct index
      LLVMConstInt(LLVMInt32TypeInContext(gen->context), fieldIdx, 0)   // Field index
    };
    LLVMValueRef fieldPtr = LLVMBuildGEP2(gen->builder, envType, envPtr, indices, 2, "env_field");

    // Store variable value into environment (capture by value)
    LLVMValueRef valueToStore = varValue;
    if (LLVMIsAAllocaInst(varValue)) {
      LLVMTypeRef allocatedType = LLVMGetAllocatedType(varValue);
      valueToStore = LLVMBuildLoad2(gen->builder, allocatedType, varValue, "captured_load");
    }
    LLVMBuildStore(gen->builder, valueToStore, fieldPtr);

    #ifdef DEBUG_CLOSURES
    fprintf(stderr, "  Stored %s at field %d\n", freeVars[i], fieldIdx);
    #endif
    fieldIdx++;
  }

  // Step 4: Create the closure function with modified signature

  // B: Properly count parameters (same fix as regular functions)
  // Parameters are ALWAYS OP_IDENTIFIER, body statements are OP_STATEMENT/OP_RETURN
  int paramCount = 0;
  while (paramCount < node->childCount &&
         node->children[paramCount]->opcode == OP_IDENTIFIER) {
    paramCount++;
  }

  #ifdef DEBUG_CLOSURES
  fprintf(stderr, "[DEBUG] Closure has %d parameters, %d total children\n",
          paramCount, node->childCount);
  #endif

  // Validate we have at least one body statement
  if (paramCount >= node->childCount) {
    fprintf(stderr, "ERROR: Closure has no body at line %d\n", node->lineNumber);
    RESTORE_PARAM_TAGS_AND_RETURN(NULL);
  }

  // B: Check if single-statement or multi-statement body
  int bodyStmtCount = node->childCount - paramCount;
  int isMultiStatementBody = (bodyStmtCount > 1);

  // Use type inference for proper function signature
  InferredFunctionType *inferredType = TypeInfer_inferFunction(node);
  if (!inferredType) {
    fprintf(stderr, "ERROR: Failed to infer closure type at line %d\n", node->lineNumber);
    RESTORE_PARAM_TAGS_AND_RETURN(NULL);
  }

  // Create parameter types array (env* + value/tag per parameter)
  int totalParams = paramCount * 2 + 1;
  LLVMTypeRef *paramTypes = malloc(totalParams * sizeof(LLVMTypeRef));
  // Accept env as erased i8* for ABI stability; cast back inside body
  paramTypes[0] = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  LLVMTypeRef tagType = LLVMInt32TypeInContext(gen->context);
  LLVMValueRef floatTagConst = LLVMConstInt(tagType, TYPE_FLOAT, 0);

  // CRITICAL: Non-env parameters are i64 (universal) paired with runtime tag
  for (int i = 0; i < paramCount; i++) {
    int paramIndex = 1 + (i * 2);
    paramTypes[paramIndex] = gen->intType;      // i64 universal value
    paramTypes[paramIndex + 1] = tagType;       // Runtime type tag (int32)
  }

  // Get inferred return type
  LLVMTypeRef returnType = TypeInfer_getLLVMType(inferredType->returnType, gen->context);

  //  If closure body returns another closure, override return type to pointer
  // Type inference doesn't handle nested closures, so we detect it here

  // B: For multi-statement bodies, check the LAST statement for closure return
  AstNode *lastBodyStmt = node->children[node->childCount - 1];
  AstNode *actualBody = lastBodyStmt;
  AstNode *returnValueNode = NULL;

  // Unwrap STATEMENT nodes - body might be wrapped
  if (lastBodyStmt && lastBodyStmt->opcode == OP_STATEMENT && lastBodyStmt->childCount > 0) {
    actualBody = lastBodyStmt->children[lastBodyStmt->childCount - 1];
  }

  if (actualBody) {
    if (actualBody->opcode == OP_RETURN && actualBody->childCount > 0) {
      returnValueNode = actualBody->children[0];
    } else {
      returnValueNode = actualBody;
    }
  }

  //  Detect if closure returns another closure (nested closures)
  int returnsAnotherClosure = 0;
  if (actualBody && actualBody->opcode == OP_FUNCTION) {
    returnType = LLVMClosures_getClosureType(gen->context);
    returnType = LLVMPointerType(returnType, 0);
    returnsAnotherClosure = 1;
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE RET TYPE] Body is OP_FUNCTION - returns closure!\n");
  #endif
  } else if (actualBody && actualBody->opcode == OP_RETURN && actualBody->childCount > 0) {
    AstNode *returnValue = actualBody->children[0];
    if (returnValue && returnValue->opcode == OP_FUNCTION) {
      returnType = LLVMClosures_getClosureType(gen->context);
      returnType = LLVMPointerType(returnType, 0);
      returnsAnotherClosure = 1;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE RET TYPE] Return value is OP_FUNCTION - returns closure!\n");
  #endif
    }
  }

  //  Store the LOGICAL return type for the tag
  LLVMTypeRef logicalReturnType = returnType;

  // DEBUG: Print return type information
  LLVMTypeKind typeKind = LLVMGetTypeKind(logicalReturnType);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE RET TYPE] logicalReturnType kind = %d (8=int, 3=double, 12=ptr)\n", typeKind);
  #endif

  //  Check if closure returns a parameter (polymorphic identity pattern)
  int returnsParameter = 0;
  int returnedParamIndex = 0;  // Which parameter is being returned (0-based)
  if (actualBody && actualBody->opcode == OP_RETURN && actualBody->childCount > 0) {
    AstNode *returnValue = actualBody->children[0];
    if (returnValue && returnValue->opcode == OP_IDENTIFIER && returnValue->val) {
      // Check if this identifier matches any parameter name
      for (int i = 0; i < paramCount; i++) {
        AstNode *param = node->children[i];
        if (param && param->val && strcmp(returnValue->val, param->val) == 0) {
          returnsParameter = 1;
          returnedParamIndex = i;  // Store which parameter index is being returned
  #if 0  // Debug output disabled
          fprintf(stderr, "[CLOSURE RET TYPE] Returns parameter '%s' (index %d) - using DYNAMIC tag!\n",
                  returnValue->val, i);
  #endif
          break;
        }
      }
    }
  }

  int returnTypeTag;
  if (returnsAnotherClosure) {
    //  Use special tag for closure returns (for nested closures)
    returnTypeTag = CLOSURE_RETURN_CLOSURE;  // 3
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE RET TYPE] Determined tag = 3 (CLOSURE) - nested closure pattern\n");
  #endif
  } else if (returnsParameter) {
    //  Use dynamic tag for polymorphic identity functions
    returnTypeTag = CLOSURE_RETURN_DYNAMIC;  // 5
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE RET TYPE] Determined tag = 5 (DYNAMIC) - polymorphic return\n");
  #endif
  } else {
    returnTypeTag = LLVMClosures_getReturnTypeTag(gen, logicalReturnType);
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE RET TYPE] Determined tag = %d (0=INT, 1=FLOAT, 2=PTR)\n", returnTypeTag);
  #endif
  }

  //  CRITICAL FIX - Always compile with i8* return type (universal)
  // Then cast actual values to/from i8* based on type tag
  // This ensures call signatures match between compilation and invocation
  LLVMTypeRef universalReturnType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Create function type with universal return
  LLVMTypeRef funcType = LLVMFunctionType(universalReturnType, paramTypes, totalParams, 0);

  // Generate unique closure function name
  static int closureCounter = 0;
  char funcName[64];
  snprintf(funcName, sizeof(funcName), "_franz_closure_%d", closureCounter++);

  // Create function
  LLVMValueRef closureFunc = LLVMAddFunction(gen->module, funcName, funcType);

  // Save current builder state
  LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(gen->builder);
  LLVMValueRef savedFunction = gen->currentFunction;

  // Create entry block for closure function
  LLVMBasicBlockRef entryBlock = LLVMAppendBasicBlockInContext(gen->context, closureFunc, "entry");
  LLVMPositionBuilderAtEnd(gen->builder, entryBlock);

  // Set current function for nested compilation
  gen->currentFunction = closureFunc;

  // Save current variable map
  LLVMVariableMap *savedVariables = gen->variables;
  gen->variables = LLVMVariableMap_new();
  int prevClosureTagContext = gen->currentClosureReturnTag;

  // FIX #5: Save isPolymorphicFunction to prevent ABI mismatch (ptr vs i64)
  // Closures should NOT be polymorphic - they always return ptr
  int savedIsPolymorphic = gen->isPolymorphicFunction;
  gen->isPolymorphicFunction = 0;  // Closures use uniform ptr return type
  gen->currentClosureReturnTag = returnTypeTag;
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE SET TAG] returnTypeTag=%d\n", returnTypeTag);
  #endif

  // Get environment parameter (first parameter)
  LLVMValueRef envParam = LLVMGetParam(closureFunc, 0);
  LLVMSetValueName(envParam, "env");
  // Cast erased env (i8*) back to concrete env type
  LLVMValueRef envTyped = LLVMBuildPointerCast(
      gen->builder,
      envParam,
      LLVMPointerType(envType, 0),
      "env_typed");

  // Load captured variables from environment into variable map
  // CRITICAL FIX: Skip variables in gen->functions - they're already accessible
  int loadFieldIdx = 0;
  for (int i = 0; i < freeVarsCount; i++) {
    // Skip if variable is in functions map (already accessible)
    if (LLVMVariableMap_get(gen->functions, freeVars[i])) {
      // No need to load - function is still accessible via gen->functions
      continue;
    }

    // Get pointer to field in environment struct (using loadFieldIdx, not i)
    LLVMValueRef indices[2] = {
      LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),  // Struct index
      LLVMConstInt(LLVMInt32TypeInContext(gen->context), loadFieldIdx, 0)   // Field index
    };
    LLVMValueRef fieldPtr = LLVMBuildGEP2(gen->builder, envType, envTyped, indices, 2, freeVars[i]);

    // Load the value
    LLVMValueRef fieldValue = LLVMBuildLoad2(gen->builder, LLVMStructGetTypeAtIndex(envType, loadFieldIdx), fieldPtr, freeVars[i]);

    // Register in variable map (so function body can access it)
    LLVMVariableMap_set(gen->variables, freeVars[i], fieldValue);

    #ifdef DEBUG_CLOSURES
    fprintf(stderr, "  Loaded captured variable '%s' from environment field %d\n", freeVars[i], loadFieldIdx);
    #endif
    loadFieldIdx++;
  }

  // Add regular function parameters to variable map
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE COMPILE] Adding %d parameters to variable map (with i64→typed conversions)\n", paramCount);
  #endif
  for (int i = 0; i < paramCount; i++) {
    AstNode *paramNode = node->children[i];
    if (!paramNode || !paramNode->val) {
      fprintf(stderr, "ERROR: Invalid parameter at index %d\n", i);
      continue;
    }

    // Raw universal i64 parameter (after env) + runtime tag
    int paramValueIndex = 1 + (i * 2);
    LLVMValueRef rawParam = LLVMGetParam(closureFunc, paramValueIndex);
    LLVMValueRef tagParam = LLVMGetParam(closureFunc, paramValueIndex + 1);
    LLVMSetValueName(rawParam, paramNode->val);
    if (tagParam && paramNode->val) {
      char tagName[128];
      snprintf(tagName, sizeof(tagName), "%s_tag", paramNode->val);
      LLVMSetValueName(tagParam, tagName);
    }
    if (gen->paramTypeTags && paramNode->val) {
      LLVMVariableMap_set(gen->paramTypeTags, paramNode->val, tagParam);
    }

    // Determine expected type via inference and convert from i64
    LLVMTypeRef expectedType = TypeInfer_getLLVMType(inferredType->paramTypes[i], gen->context);
    LLVMTypeKind expectedKind = LLVMGetTypeKind(expectedType);
    LLVMValueRef typedParam = rawParam;

    if (expectedKind == LLVMPointerTypeKind) {
      // i64 → pointer
      typedParam = LLVMBuildIntToPtr(gen->builder, rawParam, expectedType, "arg_i64_to_ptr");
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Param '%s': i64→ptr conversion applied\n", paramNode->val);
  #endif
    } else if (expectedType == gen->floatType) {
      // Use runtime tag to determine whether raw bits are float or int
      LLVMValueRef isFloatArg =
          LLVMBuildICmp(gen->builder, LLVMIntEQ, tagParam, floatTagConst, "arg_is_float");
      LLVMValueRef bitcastValue =
          LLVMBuildBitCast(gen->builder, rawParam, gen->floatType, "arg_i64_to_double_bits");
      LLVMValueRef widenedInt =
          LLVMBuildSIToFP(gen->builder, rawParam, gen->floatType, "arg_int_to_double");
      typedParam = LLVMBuildSelect(gen->builder, isFloatArg, bitcastValue, widenedInt,
                                   "arg_float_value");
      // Debug output disabled
      // fprintf(stderr,
      //         "[CLOSURE COMPILE] Param '%s': runtime-tag-aware conversion to double applied\n",
      //         paramNode->val);
    } else if (expectedType == gen->intType) {
      // Convert runtime float payloads back to integers when necessary
      LLVMValueRef isFloatArg =
          LLVMBuildICmp(gen->builder, LLVMIntEQ, tagParam, floatTagConst, "arg_float_for_int");
      LLVMValueRef floatBits =
          LLVMBuildBitCast(gen->builder, rawParam, gen->floatType, "arg_i64_to_double_for_int");
      LLVMValueRef truncated =
          LLVMBuildFPToSI(gen->builder, floatBits, gen->intType, "arg_double_to_int");
      typedParam = LLVMBuildSelect(gen->builder, isFloatArg, truncated, rawParam, "arg_int_value");
      // Debug output disabled
      // fprintf(stderr,
      //         "[CLOSURE COMPILE] Param '%s': runtime-tag-aware conversion to int applied\n",
      //         paramNode->val);
    } else {
      // Keep as i64 integer
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Param '%s': kept as i64\n", paramNode->val);
  #endif
    }

  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE] Registered parameter '%s' at LLVM param index %d (raw=%p, typed=%p)\n",
            paramNode->val, i+1, (void*)rawParam, (void*)typedParam);
  #endif

    // Register parameter in variable map (typed)
    LLVMVariableMap_set(gen->variables, paramNode->val, typedParam);

    // CRITICAL FIX: Closure parameters are RAW i64 values with runtime tags, NOT Generic* pointers
    // Do NOT mark them as Generic* - this causes segfaults when unboxing tries to dereference them
    // The runtime passes actual primitive values (e.g., -5), not Generic* pointers
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE] Parameter '%s' registered as raw i64 (not Generic*)\n", paramNode->val);
  #endif
  }

  // B: Compile closure body (single or multi-statement)
  LLVMValueRef bodyValue = NULL;

  if (isMultiStatementBody) {
    // Multi-statement body: compile each statement in sequence
    #ifdef DEBUG_CLOSURES
    fprintf(stderr, "[DEBUG] Compiling multi-statement closure body with %d statements\n", bodyStmtCount);
    #endif

    for (int i = paramCount; i < node->childCount; i++) {
      AstNode *stmt = node->children[i];
      LLVMValueRef stmtValue = LLVMCodeGen_compileNode(gen, stmt);

      if (!stmtValue) {
        fprintf(stderr, "ERROR: Failed to compile statement %d in closure body at line %d\n",
                i - paramCount + 1, stmt->lineNumber);
        LLVMVariableMap_free(gen->variables);
        gen->variables = savedVariables;
        gen->currentFunction = savedFunction;
        LLVMPositionBuilderAtEnd(gen->builder, savedBlock);
        TypeInfer_freeInferredType(inferredType);
        free(paramTypes);
        RESTORE_PARAM_TAGS_AND_RETURN(NULL);
      }

      // Keep the last statement's value as the closure return value
      bodyValue = stmtValue;
    }
  } else {
    // Single-statement body (original behavior)
    AstNode *singleBodyStmt = node->children[paramCount];

    #ifdef DEBUG_CLOSURES
    fprintf(stderr, "[DEBUG] Compiling single-statement closure body\n");
    #endif

    bodyValue = LLVMCodeGen_compileNode(gen, singleBodyStmt);

    if (!bodyValue) {
      fprintf(stderr, "ERROR: Failed to compile closure body at line %d\n", node->lineNumber);
      LLVMVariableMap_free(gen->variables);
      gen->variables = savedVariables;
      gen->currentFunction = savedFunction;
      LLVMPositionBuilderAtEnd(gen->builder, savedBlock);
      TypeInfer_freeInferredType(inferredType);
      free(paramTypes);
      RESTORE_PARAM_TAGS_AND_RETURN(NULL);
    }
  }

  // Check if we need to add a return
  LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
  LLVMValueRef terminator = LLVMGetBasicBlockTerminator(currentBlock);

  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE RETURN CHECK] Closure at line %d: currentBlock=%p, terminator=%p, bodyValue=%p\n",
          node->lineNumber, (void*)currentBlock, (void*)terminator, (void*)bodyValue);
  #endif

  LLVMTypeRef actualReturnType = NULL;
  LLVMTypeKind actualKind = LLVMIntegerTypeKind;
  if (!terminator) {
    //  Cast actual value to universal i8* before returning
    actualReturnType = LLVMTypeOf(bodyValue);
    actualKind = LLVMGetTypeKind(actualReturnType);

    // If metadata says FLOAT but LLVM emitted INT, convert to float before boxing.
    if (returnTypeTag == CLOSURE_RETURN_FLOAT &&
        actualKind == LLVMIntegerTypeKind) {
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Converting integer return to float to match inferred type\n");
  #endif
      bodyValue = LLVMBuildSIToFP(gen->builder, bodyValue, gen->floatType, "ret_itof_closure");
      actualReturnType = LLVMTypeOf(bodyValue);
      actualKind = LLVMGetTypeKind(actualReturnType);
    }

    // If metadata says INT but LLVM emitted FLOAT, convert accordingly.
    if (returnTypeTag == CLOSURE_RETURN_INT &&
        (actualKind == LLVMFloatTypeKind || actualKind == LLVMDoubleTypeKind)) {
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Converting float return to int to match inferred type\n");
  #endif
      bodyValue = LLVMBuildFPToSI(gen->builder, bodyValue, gen->intType, "ret_ftoi_closure");
      actualReturnType = LLVMTypeOf(bodyValue);
      actualKind = LLVMGetTypeKind(actualReturnType);
    }

    int actualReturnTag = LLVMClosures_getReturnTypeTag(gen, actualReturnType);
    int actualTagIsConcrete = (actualReturnTag == CLOSURE_RETURN_INT ||
                               actualReturnTag == CLOSURE_RETURN_FLOAT ||
                               actualReturnTag == CLOSURE_RETURN_POINTER);

    // FIX: Do NOT override inferred INT/FLOAT tags
    // If type inference determined the function returns INT or FLOAT,
    // trust that inference instead of the actual LLVM type (which might be Generic*)
    // This is the industry-standard pattern (OCaml, Haskell, Rust)
    int InferredConcrete = (returnTypeTag == CLOSURE_RETURN_INT ||
                                  returnTypeTag == CLOSURE_RETURN_FLOAT);

    int canOverrideReturnTag = (returnTypeTag != CLOSURE_RETURN_DYNAMIC &&
                                returnTypeTag != CLOSURE_RETURN_CLOSURE &&
                                !InferredConcrete);  // ← Don't override inference

    if (gen->currentFunctionName &&
        (strcmp(gen->currentFunctionName, "simple_test") == 0 ||
         strcmp(gen->currentFunctionName, "grade_test") == 0)) {
      fprintf(stderr, "[DEBUG CLOSURE RET TYPE] %s tag=%d actualTag=%d actualKind=%d\n",
              gen->currentFunctionName, returnTypeTag, actualReturnTag, actualKind);
    }
    if (canOverrideReturnTag && actualTagIsConcrete && returnTypeTag != actualReturnTag) {
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Aligning returnTypeTag with actual type: %d -> %d\n",
              returnTypeTag, actualReturnTag);
  #endif
      returnTypeTag = actualReturnTag;
      logicalReturnType = actualReturnType;
    } else if (InferredConcrete && returnTypeTag != actualReturnTag) {
      //  Type inference says INT/FLOAT, but LLVM says POINTER
      // This is expected when calling stdlib functions that are boxed
      // Keep the inferred tag and trust it
    }

  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE] bodyValue = %p, type kind = %d\n", (void*)bodyValue, actualKind);
  #endif
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE] bodyValue name = '%s'\n", LLVMGetValueName(bodyValue) ? LLVMGetValueName(bodyValue) : "(null)");
  #endif

    LLVMValueRef universalValue;
    if (actualKind == LLVMPointerTypeKind) {
      // Already a pointer - just cast to i8*
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Return type: Pointer, casting to i8*\n");
  #endif
      universalValue = LLVMBuildPointerCast(gen->builder, bodyValue, universalReturnType, "to_universal_ptr");
    } else if (actualKind == LLVMIntegerTypeKind) {
      // Integers of any width: extend to i64 before inttoptr so printf/puts (i32) return values are safe
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Return type: Integer (%u bits), normalizing to i64\n",
              LLVMGetIntTypeWidth(actualReturnType));
  #endif
      LLVMValueRef widened = bodyValue;
      if (actualReturnType != gen->intType) {
        widened = LLVMBuildIntCast2(gen->builder, bodyValue, gen->intType,
                                    /*isSigned*/ 1, "int_cast_i64");
      }
      universalValue = LLVMBuildIntToPtr(gen->builder, widened, universalReturnType, "int_to_ptr");
    } else if (actualKind == LLVMFloatTypeKind || actualKind == LLVMDoubleTypeKind) {
      // Float - bitcast to i64 first, then inttoptr
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Return type: Float, bitcast then inttoptr\n");
  #endif
      LLVMValueRef asInt = LLVMBuildBitCast(gen->builder, bodyValue, gen->intType, "float_as_int");
      universalValue = LLVMBuildIntToPtr(gen->builder, asInt, universalReturnType, "float_to_ptr");
    } else {
      // Unknown type - try direct cast
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE COMPILE] Return type: Unknown (%d), trying pointer cast\n", actualKind);
  #endif
      universalValue = LLVMBuildPointerCast(gen->builder, bodyValue, universalReturnType, "unknown_to_ptr");
    }

  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE] universalValue = %p (ready to return as i8*)\n", (void*)universalValue);
  #endif
    // Add return with universal value
    LLVMBuildRet(gen->builder, universalValue);
  } else if (LLVMIsAReturnInst(terminator)) {
    LLVMValueRef retOperand = LLVMGetOperand(terminator, 0);
    if (retOperand) {
      actualReturnType = LLVMTypeOf(retOperand);
      actualKind = LLVMGetTypeKind(actualReturnType);
    }
  }

  if (!actualReturnType) {
    actualReturnType = logicalReturnType;
    actualKind = LLVMGetTypeKind(actualReturnType);
  }

  int actualReturnTag = LLVMClosures_getReturnTypeTag(gen, actualReturnType);
  int actualTagIsConcrete = (actualReturnTag == CLOSURE_RETURN_INT ||
                             actualReturnTag == CLOSURE_RETURN_FLOAT ||
                             actualReturnTag == CLOSURE_RETURN_POINTER);

  // FIX: Do NOT override inferred INT/FLOAT tags (with return statement path)
  // This is the same fix as lines 599-611, but for closures WITH explicit return statements
  int InferredConcrete = (returnTypeTag == CLOSURE_RETURN_INT ||
                                returnTypeTag == CLOSURE_RETURN_FLOAT);

  int canOverrideReturnTag = (returnTypeTag != CLOSURE_RETURN_DYNAMIC &&
                              returnTypeTag != CLOSURE_RETURN_CLOSURE &&
                              !InferredConcrete);  // ← Don't override inference

  if (canOverrideReturnTag && actualTagIsConcrete && returnTypeTag != actualReturnTag) {
  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE COMPILE] Aligning returnTypeTag with actual type: %d -> %d\n",
            returnTypeTag, actualReturnTag);
  #endif
    returnTypeTag = actualReturnTag;
    logicalReturnType = actualReturnType;
  } else if (InferredConcrete && returnTypeTag != actualReturnTag) {
    //  Type inference says INT/FLOAT, but LLVM says POINTER
    // Keep the inferred tag and trust it (industry-standard type-driven compilation)
  }

  // After compiling the body, re-check whether the return expression is a Generic* value.
  // Type inference often reports i64 for closure-call results even though they hold boxed Generic*
  // pointers. If the AST node is tracked as Generic*, force the return tag to POINTER so that
  // callers keep the boxed value instead of reinterpreting it as an integer address.
  if (!returnsAnotherClosure && !returnsParameter && returnValueNode) {
    int returnIsGeneric = isGenericPointerNode(gen, returnValueNode);
    if (gen->debugMode) {
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE RET TYPE] Return node opcode=%d, isGenericPointer=%d\n",
              returnValueNode->opcode, returnIsGeneric);
  #endif
    }
    if (returnIsGeneric && returnTypeTag != CLOSURE_RETURN_POINTER) {
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE RET TYPE] Return expression tracked as Generic* - forcing POINTER tag\n");
  #endif
      returnTypeTag = CLOSURE_RETURN_POINTER;
      logicalReturnType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    }
  }

  // Restore builder state and variable map
  LLVMVariableMap_free(gen->variables);
  gen->variables = savedVariables;
  gen->currentClosureReturnTag = prevClosureTagContext;
  gen->currentFunction = savedFunction;
  gen->isPolymorphicFunction = savedIsPolymorphic;  // FIX #5: Restore polymorphic flag
  LLVMPositionBuilderAtEnd(gen->builder, savedBlock);

  TypeInfer_freeInferredType(inferredType);
  free(paramTypes);

  // Step 5: Create closure struct { func_ptr, env_ptr }
  LLVMTypeRef closureType = LLVMClosures_getClosureType(gen->context);
  LLVMValueRef closurePtr = LLVMBuildMalloc(gen->builder, closureType, "closure");

  // Cast function pointer to i8*
  LLVMValueRef funcPtrCast = LLVMBuildPointerCast(gen->builder, closureFunc,
                                                   LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
                                                   "func_ptr");

  // Cast environment pointer to i8*
  LLVMValueRef envPtrCast = LLVMBuildPointerCast(gen->builder, envPtr,
                                                  LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
                                                  "env_ptr");

  // Store function pointer in closure struct (field 0)
  LLVMValueRef funcFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0)
  };
  LLVMValueRef funcFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, funcFieldIndices, 2, "func_field");
  LLVMBuildStore(gen->builder, funcPtrCast, funcFieldPtr);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE DEBUG] Stored funcPtr at field 0, value=%p\n", (void*)LLVMConstPtrToInt(funcPtrCast, gen->intType));
  #endif

  // Store environment pointer in closure struct (field 1)
  LLVMValueRef envFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 1, 0)
  };
  LLVMValueRef envFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, envFieldIndices, 2, "env_field");
  LLVMBuildStore(gen->builder, envPtrCast, envFieldPtr);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE DEBUG] Stored envPtr at field 1\n");
  #endif

  //  Store return type tag in closure struct (field 2)
  // This enables industry-standard type-safe closure calls (like Rust/C++)
  // Use logicalReturnType (the inferred type), not universalReturnType
  LLVMValueRef returnTypeTagValue = LLVMConstInt(LLVMInt32TypeInContext(gen->context), returnTypeTag, 0);

  LLVMValueRef tagFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 2, 0)  // Field 2 = return type tag
  };
  LLVMValueRef tagFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, tagFieldIndices, 2, "tag_field");
  LLVMBuildStore(gen->builder, returnTypeTagValue, tagFieldPtr);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE DEBUG] Stored returnTypeTag=%d at field 2\n", returnTypeTag);
  #endif

  //  Store parameter index for DYNAMIC tag (field 3)
  LLVMValueRef paramIndexValue = LLVMConstInt(LLVMInt32TypeInContext(gen->context), returnedParamIndex, 0);
  LLVMValueRef paramIndexFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 3, 0)  // Field 3 = parameter index
  };
  LLVMValueRef paramIndexFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, paramIndexFieldIndices, 2, "param_index_field");
  LLVMBuildStore(gen->builder, paramIndexValue, paramIndexFieldPtr);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE DEBUG] Stored paramIndex=%d at field 3\n", returnedParamIndex);
  #endif

  #ifdef DEBUG_CLOSURES
  fprintf(stderr, "[DEBUG] Created closure struct with return type tag = %d\n", returnTypeTag);
  #endif

  // CRITICAL: Convert closure pointer to i64 for universal representation
  // All LLVM values in Franz are passed as i64 for compatibility
  LLVMValueRef closureAsInt = LLVMBuildPtrToInt(gen->builder, closurePtr, gen->intType, "closure_as_i64");

#undef RESTORE_PARAM_TAGS_AND_RETURN
  LLVMClosures_restoreParamTagState(gen, savedParamTagMap, closureParamTagMap);
  return closureAsInt;
}

// ============================================================================
// Closure Call
// ============================================================================

LLVMValueRef LLVMClosures_callClosure(LLVMCodeGen *gen, LLVMValueRef closureValue, LLVMValueRef *args, int argCount, AstNode **argNodes) {
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Entry: argCount = %d\n", argCount);
  #endif
  // Extract closure struct type
  LLVMTypeRef closureType = LLVMClosures_getClosureType(gen->context);

  //  closureValue is now i64 (universal representation)
  // Convert back to pointer for field extraction
  LLVMTypeRef closureValueType = LLVMTypeOf(closureValue);
  LLVMTypeKind closureValueKind = LLVMGetTypeKind(closureValueType);

  LLVMValueRef closurePtr;
  if (closureValueKind == LLVMIntegerTypeKind) {
    // Convert i64 → closure struct pointer
    closurePtr = LLVMBuildIntToPtr(gen->builder, closureValue,
                                     LLVMPointerType(closureType, 0),
                                     "closure_int_to_ptr");
  } else {
    // Already a pointer
    closurePtr = closureValue;
  }
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Extracted closurePtr\n");
  #endif

  // Extract function pointer from closure struct (field 0)
  LLVMValueRef funcFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0)
  };
  LLVMValueRef funcFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, funcFieldIndices, 2, "func_field");
  LLVMValueRef funcPtr = LLVMBuildLoad2(gen->builder, LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0), funcFieldPtr, "func_ptr");
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Extracted funcPtr\n");
  #endif

  // Extract environment pointer from closure struct (field 1)
  LLVMValueRef envFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 1, 0)
  };
  LLVMValueRef envFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, envFieldIndices, 2, "env_field");
  LLVMValueRef envPtr = LLVMBuildLoad2(gen->builder, LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0), envFieldPtr, "env_ptr");
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Extracted envPtr\n");
  #endif

  //  Extract return type tag from closure struct (field 2)
  // INDUSTRY-STANDARD FIX (OCaml approach): ALL functions accept env* as first parameter
  // Uniform calling convention: func(env*, arg1, arg2, ...)
  // Regular functions ignore env* (NULL), closures use it
  //
  // CRITICAL FIX: Convert ALL arguments to i64 (Franz's universal type)
  // The wrapper expects i64 for all non-env parameters, but args might be i8* (strings), i64 (ints), etc.
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Allocating augmentedArgs for %d+1 arguments\n", argCount);
  #endif
  int augmentedCount = argCount * 2 + 1;
  LLVMValueRef *augmentedArgs = malloc(augmentedCount * sizeof(LLVMValueRef));
  augmentedArgs[0] = envPtr;  // Environment as first argument (may be NULL for regular functions)
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Set augmentedArgs[0] = envPtr\n");
  #endif

  //  Track argument types for polymorphic closure support
  // For identity functions {x -> <- x}, we need the argument type to box returns correctly
  int *argTypeTags = malloc(argCount * sizeof(int));

  LLVMTypeRef tagType = LLVMInt32TypeInContext(gen->context);

  for (int i = 0; i < argCount; i++) {
    int argValueIndex = 1 + (i * 2);
    LLVMValueRef currentArg = args[i] ? args[i] : LLVMConstInt(gen->intType, 0, 0);
    LLVMTypeRef argType = args[i] ? LLVMTypeOf(args[i]) : gen->intType;
    LLVMTypeKind argKind = LLVMGetTypeKind(argType);

    int explicitTag = (argNodes && LLVMClosures_nodeIsVoid(gen, argNodes[i]))
                          ? CLOSURE_RETURN_VOID
                          : -1;

    if (explicitTag == CLOSURE_RETURN_VOID) {
      augmentedArgs[argValueIndex] = currentArg;
      argTypeTags[i] = CLOSURE_RETURN_VOID;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE CALL ARG] Argument %d flagged as VOID literal\n", i);
  #endif
    } else if (argNodes && argNodes[i] && isGenericPointerNode(gen, argNodes[i])) {
      //  Semantic type check - lists/Generic* values
      // Lists and Generic* are returned as ptr, but closures need i64
      // This handles: list literals, list operations, closure results, tracked variables
      if (argKind == LLVMPointerTypeKind) {
        // Convert Generic* pointer to i64
        augmentedArgs[argValueIndex] = LLVMBuildPtrToInt(gen->builder, currentArg, gen->intType, "generic_to_i64");
      } else {
        // Already i64 (stored Generic* value)
        augmentedArgs[argValueIndex] = currentArg;
      }
      argTypeTags[i] = CLOSURE_RETURN_POINTER;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE CALL ARG] Argument %d is Generic* (list/closure), tag=POINTER\n", i);
  #endif
    } else if (argKind == LLVMPointerTypeKind) {
      // Convert pointer (string, etc.) to i64
      augmentedArgs[argValueIndex] = LLVMBuildPtrToInt(gen->builder, currentArg, gen->intType, "arg_ptr_to_i64");
      argTypeTags[i] = CLOSURE_RETURN_POINTER;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE CALL ARG] Converted pointer argument %d to i64, tag=POINTER\n", i);
  #endif
    } else if (argKind == LLVMIntegerTypeKind) {
      // Already i64, use as-is (plain integers, not Generic*)
      augmentedArgs[argValueIndex] = currentArg;
      argTypeTags[i] = CLOSURE_RETURN_INT;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE CALL ARG] Argument %d is already i64, tag=INT\n", i);
  #endif
    } else if (argKind == LLVMDoubleTypeKind || argKind == LLVMFloatTypeKind) {
      // Convert float to i64 (bitcast)
      LLVMValueRef asInt = LLVMBuildBitCast(gen->builder, currentArg, gen->intType, "arg_float_to_i64");
      augmentedArgs[argValueIndex] = asInt;
      argTypeTags[i] = CLOSURE_RETURN_FLOAT;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE CALL ARG] Converted float argument %d to i64, tag=FLOAT\n", i);
  #endif
    } else {
      // Unknown type - try to use as-is
      augmentedArgs[argValueIndex] = currentArg;
      argTypeTags[i] = CLOSURE_RETURN_INT;
  #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE CALL ARG] WARNING: Unknown type for argument %d, defaulting to INT\n", i);
  #endif
    }

    augmentedArgs[argValueIndex + 1] =
        LLVMConstInt(tagType, (unsigned long long)argTypeTags[i], 0);
  }
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] All %d arguments converted to i64\n", argCount);
  #endif

  // Build function type for the call
  //  Industry-standard solution using runtime type tags
  // STRATEGY: Use i8* (pointer) as universal return type for the call,
  // then bitcast to the actual type based on the tag
  // ALL non-env parameters are i64 (Franz's universal type)
  LLVMTypeRef *paramTypes = malloc(augmentedCount * sizeof(LLVMTypeRef));
  paramTypes[0] = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);  // env*
  for (int i = 0; i < argCount; i++) {
    int paramIndex = 1 + (i * 2);
    paramTypes[paramIndex] = gen->intType;  // All argument values are i64 in Franz
    paramTypes[paramIndex + 1] = tagType;   // Runtime type tag per argument
  }

  // INDUSTRY-STANDARD FIX: Use i8* as universal closure return type (like trait objects / OCaml)
  // All wrappers return i8*, we convert back based on runtime type tag
  LLVMTypeRef universalReturnType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMTypeRef funcType = LLVMFunctionType(universalReturnType, paramTypes, augmentedCount, 0);

  // Cast function pointer to correct type
  LLVMValueRef typedFuncPtr = LLVMBuildPointerCast(gen->builder, funcPtr, LLVMPointerType(funcType, 0), "typed_func");
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Built typed function pointer\n");
  #endif

  // Call the closure function (returns i8*)
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] About to call closure function with %d total args (including tags)...\n", augmentedCount);
  #endif
  LLVMValueRef rawResult = LLVMBuildCall2(gen->builder, funcType, typedFuncPtr, augmentedArgs, augmentedCount, "closure_call");
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] ✓ Call succeeded! rawResult obtained\n");
  #endif

  // TCO: Mark as tail call if in tail position
  if (gen->enableTCO && gen->inTailPosition) {
    LLVMSetTailCall(rawResult, 1);
    if (gen->debugMode) {
      fprintf(stderr, "[TCO] Marked tail call\n");
    }
  }

  // Load return type tag to determine how to convert the result
  // CRITICAL FIX: Use BuildGEP2 to match how the tag is stored (not BuildStructGEP2)
  LLVMValueRef tagFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 2, 0)  // Field 2 = return type tag
  };
  LLVMValueRef tagFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, tagFieldIndices, 2, "tag_field_ptr");
  LLVMValueRef returnTag = LLVMBuildLoad2(gen->builder, LLVMInt32TypeInContext(gen->context), tagFieldPtr, "return_tag");
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Loaded returnTag from closure struct (runtime value will be checked by switch)\n");
  #endif

  //  Load parameter index for DYNAMIC tag (field 3)
  // CRITICAL FIX: Use BuildGEP2 to match storage pattern
  LLVMValueRef paramIndexFieldIndices[2] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 3, 0)  // Field 3 = parameter index
  };
  LLVMValueRef paramIndexFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, paramIndexFieldIndices, 2, "param_index_field_ptr");
  LLVMValueRef paramIndex = LLVMBuildLoad2(gen->builder, LLVMInt32TypeInContext(gen->context), paramIndexFieldPtr, "param_index");

  //  Handle DYNAMIC return tag (polymorphic closures)
  // If return tag is DYNAMIC (5), use the specified parameter's runtime tag
  LLVMValueRef actualReturnTag = returnTag;
  if (argCount > 0) {
    // Create blocks for DYNAMIC check
    LLVMBasicBlockRef checkDynamicBlock = LLVMGetInsertBlock(gen->builder);
    LLVMValueRef checkFunc = LLVMGetBasicBlockParent(checkDynamicBlock);
    LLVMBasicBlockRef isDynamicBlock = LLVMAppendBasicBlockInContext(gen->context, checkFunc, "is_dynamic");
    LLVMBasicBlockRef notDynamicBlock = LLVMAppendBasicBlockInContext(gen->context, checkFunc, "not_dynamic");
    LLVMBasicBlockRef tagMergeBlock = LLVMAppendBasicBlockInContext(gen->context, checkFunc, "tag_merge");

    // Check if returnTag == CLOSURE_RETURN_DYNAMIC (5)
    LLVMValueRef isDynamic = LLVMBuildICmp(gen->builder, LLVMIntEQ, returnTag,
                                           LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_DYNAMIC, 0),
                                           "is_dynamic");
    LLVMBuildCondBr(gen->builder, isDynamic, isDynamicBlock, notDynamicBlock);

    // DYNAMIC block: Use specified parameter's tag
    LLVMPositionBuilderAtEnd(gen->builder, isDynamicBlock);
    // paramIndex is 0-based, augmentedArgs layout: [env, arg0_val, arg0_tag, arg1_val, arg1_tag, ...]
    // So for parameter i, tag is at augmentedArgs[2 + i*2]

    // Build switch on paramIndex to select correct tag (supports up to 8 parameters)
    LLVMBasicBlockRef paramSwitchCases[8];
    LLVMValueRef paramTags[8];
    int maxParams = (argCount < 8) ? argCount : 8;

    for (int i = 0; i < maxParams; i++) {
      paramSwitchCases[i] = LLVMAppendBasicBlockInContext(gen->context, checkFunc, "param_case");
    }
    LLVMBasicBlockRef paramMergeBlock = LLVMAppendBasicBlockInContext(gen->context, checkFunc, "param_merge");

    // Switch on paramIndex
    LLVMValueRef paramSwitch = LLVMBuildSwitch(gen->builder, paramIndex, paramSwitchCases[0], maxParams);
    for (int i = 0; i < maxParams; i++) {
      LLVMAddCase(paramSwitch, LLVMConstInt(LLVMInt32TypeInContext(gen->context), i, 0), paramSwitchCases[i]);
    }

    // For each parameter index, load the corresponding tag
    for (int i = 0; i < maxParams; i++) {
      LLVMPositionBuilderAtEnd(gen->builder, paramSwitchCases[i]);
      paramTags[i] = augmentedArgs[2 + i*2];  // Tag for parameter i
      LLVMBuildBr(gen->builder, paramMergeBlock);
    }

    // Merge the selected tag
    LLVMPositionBuilderAtEnd(gen->builder, paramMergeBlock);
    LLVMValueRef paramTag = LLVMBuildPhi(gen->builder, LLVMInt32TypeInContext(gen->context), "selected_param_tag");
    LLVMAddIncoming(paramTag, paramTags, paramSwitchCases, maxParams);

  #if 0  // Debug output disabled
    fprintf(stderr, "[CLOSURE CALL] DYNAMIC tag detected - using parameter's runtime tag\n");
  #endif
    LLVMBuildBr(gen->builder, tagMergeBlock);

    // NOT DYNAMIC block: Use closure's static tag
    LLVMPositionBuilderAtEnd(gen->builder, notDynamicBlock);
    LLVMBuildBr(gen->builder, tagMergeBlock);

    // MERGE: PHI to select actual tag
    LLVMPositionBuilderAtEnd(gen->builder, tagMergeBlock);
    LLVMValueRef tagPhi = LLVMBuildPhi(gen->builder, LLVMInt32TypeInContext(gen->context), "actual_return_tag");
    LLVMValueRef tagIncomingValues[] = { paramTag, returnTag };
    LLVMBasicBlockRef tagIncomingBlocks[] = { paramMergeBlock, notDynamicBlock };  // paramMergeBlock, not isDynamicBlock
    LLVMAddIncoming(tagPhi, tagIncomingValues, tagIncomingBlocks, 2);
    actualReturnTag = tagPhi;
  }

  // Convert result based on tag:
  // - INT (tag=0): i8* → i64 (ptrtoint) - reverses the wrapper's inttoptr
  // - FLOAT (tag=1): i8* → i64 → double (ptrtoint + bitcast)
  // - POINTER (tag=2): keep as i8* - matches original pointer type

  // Create basic blocks for conditional conversion
  LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
  LLVMValueRef currentFunc = LLVMGetBasicBlockParent(currentBlock);

  LLVMBasicBlockRef intBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "convert_int");
  LLVMBasicBlockRef floatBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "convert_float");
  LLVMBasicBlockRef ptrBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "keep_ptr");
  LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "merge");

  // Switch on return tag (uses actualReturnTag which may be dynamic)
  LLVMValueRef switchInst = LLVMBuildSwitch(gen->builder, actualReturnTag, ptrBlock, 3);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_INT, 0), intBlock);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_FLOAT, 0), floatBlock);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_POINTER, 0), ptrBlock);

  // INT block: Convert i8* → i64 (reverse the wrapper's inttoptr)
  LLVMPositionBuilderAtEnd(gen->builder, intBlock);
  LLVMValueRef intResult = LLVMBuildPtrToInt(gen->builder, rawResult, gen->intType, "int_result");
  LLVMValueRef intResultAsPtr = LLVMBuildIntToPtr(gen->builder, intResult, universalReturnType, "int_as_ptr");
  LLVMBuildBr(gen->builder, mergeBlock);

  // FLOAT block: Convert i8* → i64 → double
  LLVMPositionBuilderAtEnd(gen->builder, floatBlock);
  LLVMValueRef floatAsInt = LLVMBuildPtrToInt(gen->builder, rawResult, gen->intType, "float_as_int");
  LLVMValueRef floatResult = LLVMBuildBitCast(gen->builder, floatAsInt, gen->floatType, "float_result");
  LLVMValueRef floatResultAsPtr = LLVMBuildIntToPtr(gen->builder, floatAsInt, universalReturnType, "float_as_ptr");
  LLVMBuildBr(gen->builder, mergeBlock);

  // POINTER block: Keep i8* as-is
  LLVMPositionBuilderAtEnd(gen->builder, ptrBlock);
  LLVMBuildBr(gen->builder, mergeBlock);

  // Merge block: PHI node to select result (all branches return i8* now for type consistency)
  LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
  LLVMValueRef phi = LLVMBuildPhi(gen->builder, universalReturnType, "closure_result");

  LLVMValueRef incomingValues[] = {intResultAsPtr, floatResultAsPtr, rawResult};
  LLVMBasicBlockRef incomingBlocks[] = {intBlock, floatBlock, ptrBlock};
  LLVMAddIncoming(phi, incomingValues, incomingBlocks, 3);

  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Result converted based on tag\n");
  #endif

  // ENHANCEMENT 2: Skip Generic* boxing for INT/FLOAT returns
  // If the closure returns a native INT or FLOAT, return it directly (no boxing)
  // This is the industry-standard optimization used by OCaml, Rust, and Haskell
  // Only POINTER/CLOSURE/DYNAMIC returns need Generic* boxing for type safety

  // INDUSTRY-STANDARD FIX: Box closure result into Generic* (like list operations)
  // This makes closure returns self-describing with runtime type information
  // Consistent with Rust/OCaml tagged value approach

  // Declare boxing functions (Runtime Generic* constructors)
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // franz_box_int(i64) -> Generic*
  LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
  if (!boxIntFunc) {
    LLVMTypeRef boxIntParams[] = {gen->intType};
    LLVMTypeRef boxIntType = LLVMFunctionType(genericPtrType, boxIntParams, 1, 0);
    boxIntFunc = LLVMAddFunction(gen->module, "franz_box_int", boxIntType);
  }

  // franz_box_float(double) -> Generic*
  LLVMValueRef boxFloatFunc = LLVMGetNamedFunction(gen->module, "franz_box_float");
  if (!boxFloatFunc) {
    LLVMTypeRef boxFloatParams[] = {gen->floatType};
    LLVMTypeRef boxFloatType = LLVMFunctionType(genericPtrType, boxFloatParams, 1, 0);
    boxFloatFunc = LLVMAddFunction(gen->module, "franz_box_float", boxFloatType);
  }

  // franz_box_string(i8*) -> Generic*
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
  if (!boxStringFunc) {
    LLVMTypeRef boxStringParams[] = {gen->stringType};
    LLVMTypeRef boxStringType = LLVMFunctionType(genericPtrType, boxStringParams, 1, 0);
    boxStringFunc = LLVMAddFunction(gen->module, "franz_box_string", boxStringType);
  }

  //  franz_box_closure(i8*) -> Generic* (for nested closures)
  LLVMValueRef boxClosureFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
  if (!boxClosureFunc) {
    LLVMTypeRef boxClosureParams[] = {gen->stringType};  // i8* pointer
    LLVMTypeRef boxClosureType = LLVMFunctionType(genericPtrType, boxClosureParams, 1, 0);
    boxClosureFunc = LLVMAddFunction(gen->module, "franz_box_closure", boxClosureType);
  }

  // BUGFIX: Always use closure's return tag for boxing, not argument type
  // The previous polymorphic logic confused CLOSURE_RETURN_INT (0) with "unknown type"
  // causing integer-returning closures to be misboxed based on argument types
  //  Use actualReturnTag which may be dynamically selected from parameter's tag
  LLVMValueRef finalTag = actualReturnTag;

  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Using return type tag for boxing\n");
  #endif

  // ENHANCEMENT 2: Skip boxing for INT/FLOAT returns (compile-time optimization)
  // Create blocks for conditional boxing
  LLVMBasicBlockRef checkTagBlock = LLVMGetInsertBlock(gen->builder);
  LLVMBasicBlockRef skipBoxingBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "skip_boxing");
  LLVMBasicBlockRef doBoxingBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "do_boxing");
  LLVMBasicBlockRef finalMergeBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "final_merge");

  // Check if tag is INT (0) or FLOAT (1) - these skip boxing
  // tag == 0 OR tag == 1
  LLVMValueRef isInt = LLVMBuildICmp(gen->builder, LLVMIntEQ, finalTag,
                                     LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_INT, 0),
                                     "is_int_tag");
  LLVMValueRef isFloat = LLVMBuildICmp(gen->builder, LLVMIntEQ, finalTag,
                                       LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_FLOAT, 0),
                                       "is_float_tag");
  LLVMValueRef shouldSkipBoxing = LLVMBuildOr(gen->builder, isInt, isFloat, "should_skip_boxing");
  LLVMBuildCondBr(gen->builder, shouldSkipBoxing, skipBoxingBlock, doBoxingBlock);

  // SKIP BOXING BLOCK: Return native i64/double directly
  LLVMPositionBuilderAtEnd(gen->builder, skipBoxingBlock);
  // phi is already i8*, convert to i64
  LLVMValueRef nativeResult = LLVMBuildPtrToInt(gen->builder, phi, gen->intType, "native_result");

  // DEBUG: Print the native result to verify
  if (gen->debugMode) {
    LLVMValueRef debugPrintfFunc = gen->printfFunc;
    LLVMValueRef debugFormatStr = LLVMBuildGlobalStringPtr(gen->builder, "[RUNTIME] Skip boxing, native result = %lld, tag = ", ".debug_");
    LLVMValueRef debugArgs[] = {debugFormatStr, nativeResult};
    LLVMBuildCall2(gen->builder, gen->printfType, debugPrintfFunc, debugArgs, 2, "debug__print");

    // Print the actual tag value too
    LLVMValueRef debugFormatTag = LLVMBuildGlobalStringPtr(gen->builder, "%d\n", ".debug_tag");
    LLVMValueRef debugTagArgs[] = {debugFormatTag, finalTag};
    LLVMBuildCall2(gen->builder, gen->printfType, debugPrintfFunc, debugTagArgs, 2, "debug_tag_print");
  }

  LLVMBuildBr(gen->builder, finalMergeBlock);

  // DO BOXING BLOCK: Box POINTER/CLOSURE/DYNAMIC returns
  LLVMPositionBuilderAtEnd(gen->builder, doBoxingBlock);

  // DEBUG: Print when taking boxing path
  if (gen->debugMode) {
    LLVMValueRef debugPrintf2 = gen->printfFunc;
    LLVMValueRef debugFormat2 = LLVMBuildGlobalStringPtr(gen->builder, "[RUNTIME] DO box, tag = %d\n", ".debug_dobox");
    LLVMValueRef debugArgs2[] = {debugFormat2, finalTag};
    LLVMBuildCall2(gen->builder, gen->printfType, debugPrintf2, debugArgs2, 2, "debug_dobox_print");
  }

  // Box the result based on final tag
  LLVMBasicBlockRef boxIntBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "box_int");
  LLVMBasicBlockRef boxFloatBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "box_float");
  LLVMBasicBlockRef boxPtrBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "box_ptr");
  LLVMBasicBlockRef boxClosureBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "box_closure");
  LLVMBasicBlockRef boxMergeBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, "box_merge");

  // Switch on FINAL tag to call appropriate boxing function
  //  Added case 3 for CLOSURE_RETURN_CLOSURE
  LLVMValueRef boxSwitch = LLVMBuildSwitch(gen->builder, finalTag, boxPtrBlock, 4);
  LLVMAddCase(boxSwitch, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_INT, 0), boxIntBlock);
  LLVMAddCase(boxSwitch, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_FLOAT, 0), boxFloatBlock);
  LLVMAddCase(boxSwitch, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_POINTER, 0), boxPtrBlock);
  LLVMAddCase(boxSwitch, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_CLOSURE, 0), boxClosureBlock);

  // INT: ptrtoint i8* -> i64, then box_int
  LLVMPositionBuilderAtEnd(gen->builder, boxIntBlock);
  LLVMValueRef intValue = LLVMBuildPtrToInt(gen->builder, phi, gen->intType, "int_from_ptr");
  LLVMValueRef boxedInt = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc), boxIntFunc, &intValue, 1, "boxed_int");
  LLVMBuildBr(gen->builder, boxMergeBlock);

  // FLOAT: ptrtoint i8* -> i64, bitcast i64 -> double, then box_float
  LLVMPositionBuilderAtEnd(gen->builder, boxFloatBlock);
  LLVMValueRef floatAsI64 = LLVMBuildPtrToInt(gen->builder, phi, gen->intType, "float_as_i64");
  LLVMValueRef floatValue = LLVMBuildBitCast(gen->builder, floatAsI64, gen->floatType, "float_from_i64");
  LLVMValueRef boxedFloat = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxFloatFunc), boxFloatFunc, &floatValue, 1, "boxed_float");
  LLVMBuildBr(gen->builder, boxMergeBlock);

  // POINTER (string/list/Generic*): FIX - Use smart boxing
  // The i8* could be either:
  //   1. Already a Generic* (lists) - return as-is
  //   2. Raw string pointer - box it
  // Use franz_box_pointer_smart to check and handle both cases
  LLVMPositionBuilderAtEnd(gen->builder, boxPtrBlock);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL BOXING] boxPtrBlock: using smart pointer boxing\n");
  #endif

  // Declare franz_box_pointer_smart if not declared
  LLVMValueRef boxPointerSmartFunc = LLVMGetNamedFunction(gen->module, "franz_box_pointer_smart");
  if (!boxPointerSmartFunc) {
    LLVMTypeRef params[] = {gen->stringType};  // void* parameter
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, params, 1, 0);
    boxPointerSmartFunc = LLVMAddFunction(gen->module, "franz_box_pointer_smart", funcType);
  }

  LLVMValueRef boxedString = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxPointerSmartFunc),
                                             boxPointerSmartFunc, &phi, 1, "boxed_pointer_smart");
  LLVMBuildBr(gen->builder, boxMergeBlock);

  // CLOSURE (nested closure): box_closure(i8*)
  //  CRITICAL FIX for nested closures (factory pattern)
  // When a closure returns another closure, we need franz_box_closure NOT franz_box_string
  LLVMPositionBuilderAtEnd(gen->builder, boxClosureBlock);
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL BOXING] boxClosureBlock: boxing CLOSURE pointer\n");
  #endif
  LLVMValueRef boxedClosure = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxClosureFunc), boxClosureFunc, &phi, 1, "boxed_closure");
  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL BOXING] Called franz_box_closure - CORRECT for nested closures!\n");
  #endif
  LLVMBuildBr(gen->builder, boxMergeBlock);

  // Merge: PHI node selects the boxed Generic*
  LLVMPositionBuilderAtEnd(gen->builder, boxMergeBlock);
  LLVMValueRef boxedResult = LLVMBuildPhi(gen->builder, genericPtrType, "boxed_result");
  LLVMValueRef boxedValues[] = {boxedInt, boxedFloat, boxedString, boxedClosure};
  LLVMBasicBlockRef boxedBlocks[] = {boxIntBlock, boxFloatBlock, boxPtrBlock, boxClosureBlock};
  LLVMAddIncoming(boxedResult, boxedValues, boxedBlocks, 4);

  // Convert Generic* to i64 for Franz's universal type system
  LLVMValueRef boxedResultAsI64 = LLVMBuildPtrToInt(gen->builder, boxedResult, gen->intType, "generic_as_i64");
  LLVMBuildBr(gen->builder, finalMergeBlock);

  // FINAL MERGE: PHI selects between native result (skip boxing) and boxed result
  LLVMPositionBuilderAtEnd(gen->builder, finalMergeBlock);
  LLVMValueRef finalResult = LLVMBuildPhi(gen->builder, gen->intType, "final_result");
  LLVMValueRef finalValues[] = {nativeResult, boxedResultAsI64};
  LLVMBasicBlockRef finalBlocks[] = {skipBoxingBlock, boxMergeBlock};
  LLVMAddIncoming(finalResult, finalValues, finalBlocks, 2);

  // Cleanup malloc'd arrays
  free(augmentedArgs);
  free(paramTypes);
  free(argTypeTags);

  #if 0  // Debug output disabled
  fprintf(stderr, "[CLOSURE CALL] Returning final result (boxed or native based on type)\n");
  #endif
  return finalResult;
}
