#include "type_infer.h"
#include "../tokens.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

//  Type Inference Implementation
//
// Strategy: Simple type inference based on literals and operations
//
// For each function {x y -> body}:
//   1. Analyze body to find return type
//   2. Analyze parameter usage to find parameter types
//
// Rules:
//   - Literal 3.14 → FLOAT
//   - Literal "hello" → STRING
//   - Literal 42 → INT
//   - (add x y) where x=FLOAT or y=FLOAT → result is FLOAT
//   - (add x y) where both INT → result is INT
//   - (concat x y) → both must be STRING, result is STRING
//   - Parameters default to UNKNOWN, inferred from usage

// ============================================================================
// Helper Functions
// ============================================================================

const char *TypeInfer_typeToString(InferredTypeKind kind) {
  switch (kind) {
    case INFER_TYPE_INT:     return "int";
    case INFER_TYPE_FLOAT:   return "float";
    case INFER_TYPE_STRING:  return "string";
    case INFER_TYPE_VOID:    return "void";
    case INFER_TYPE_UNKNOWN: return "unknown";
    default:                 return "invalid";
  }
}

LLVMTypeRef TypeInfer_getLLVMType(InferredTypeKind kind, LLVMContextRef context) {
  LLVMTypeRef result;
  switch (kind) {
    case INFER_TYPE_INT:
      result = LLVMInt64TypeInContext(context);
      break;
    case INFER_TYPE_FLOAT:
      result = LLVMDoubleTypeInContext(context);
      break;
    case INFER_TYPE_STRING:
      result = LLVMPointerType(LLVMInt8TypeInContext(context), 0);
      break;
    case INFER_TYPE_VOID:
      result = LLVMVoidTypeInContext(context);
      break;
    case INFER_TYPE_UNKNOWN:
      // Default to i64 for unknown types ( monomorphic only)
      result = LLVMInt64TypeInContext(context);
      break;
    default:
      result = LLVMInt64TypeInContext(context);
      break;
  }

  // Debug: Print type kind
  #if 0  // Debug output disabled
  fprintf(stderr, "[TYPE_INFER DEBUG] kind=%d -> LLVMTypeKind=%d\n",
          kind, (int)LLVMGetTypeKind(result));
  #endif

  return result;
}

InferredTypeKind TypeInfer_fromGenericType(enum Type genericType) {
  switch (genericType) {
    case TYPE_INT:    return INFER_TYPE_INT;
    case TYPE_FLOAT:  return INFER_TYPE_FLOAT;
    case TYPE_STRING: return INFER_TYPE_STRING;
    case TYPE_VOID:   return INFER_TYPE_VOID;
    default:          return INFER_TYPE_UNKNOWN;
  }
}

// ============================================================================
// Type Inference Algorithm
// ============================================================================

/**
 * Infer the type of an expression node
 * Returns the inferred type based on the node's structure
 */
static InferredTypeKind inferExpressionType(AstNode *node, AstNode *functionNode) {
  if (!node) return INFER_TYPE_UNKNOWN;

  switch (node->opcode) {
    case OP_INT:
      return INFER_TYPE_INT;

    case OP_FLOAT:
      return INFER_TYPE_FLOAT;

    case OP_STRING:
      return INFER_TYPE_STRING;

    case OP_IDENTIFIER: {
      // Check if this identifier is a parameter
      // If it is, we can't infer its type yet (UNKNOWN)
      // This will be resolved by analyzing usage
      return INFER_TYPE_UNKNOWN;
    }

    case OP_APPLICATION: {
      // Analyze function calls
      if (node->childCount == 0) return INFER_TYPE_UNKNOWN;

      AstNode *funcName = node->children[0];
      if (funcName->opcode != OP_IDENTIFIER) return INFER_TYPE_UNKNOWN;

      // Built-in function type inference
      if (strcmp(funcName->val, "add") == 0 ||
          strcmp(funcName->val, "subtract") == 0 ||
          strcmp(funcName->val, "multiply") == 0) {
        // Arithmetic: Check if any argument is float, int, or unknown
        int hasFloat = 0;
        int hasInt = 0;
        int allUnknown = 1;  // Track if ALL arguments are unknown
        for (int i = 1; i < node->childCount; i++) {
          InferredTypeKind argType = inferExpressionType(node->children[i], functionNode);
          if (argType == INFER_TYPE_FLOAT) {
            hasFloat = 1;
            allUnknown = 0;
          } else if (argType == INFER_TYPE_INT) {
            hasInt = 1;
            allUnknown = 0;
          } else if (argType != INFER_TYPE_UNKNOWN) {
            allUnknown = 0;
          }
        }

        // If any argument is explicitly float, return float
        if (hasFloat) {
          return INFER_TYPE_FLOAT;
        }
        // If at least one argument is INT and no FLOAT, return INT
        // (enhancement: even with unknown parameters, infer from literals)
        if (hasInt) {
          return INFER_TYPE_INT;
        }
        // Only if ALL arguments are unknown, defer to runtime
        if (allUnknown) {
          return INFER_TYPE_UNKNOWN;
        }
        // Default to int for arithmetic operations
        return INFER_TYPE_INT;
      }

      if (strcmp(funcName->val, "divide") == 0) {
        // Division always returns float (for now)
        // Could be improved to check if both args are int
        return INFER_TYPE_FLOAT;
      }

      if (strcmp(funcName->val, "join") == 0 ||
          strcmp(funcName->val, "concat") == 0) {
        // String concatenation returns string
        return INFER_TYPE_STRING;
      }

      if (strcmp(funcName->val, "type") == 0) {
        // (type value) always returns a string label
        return INFER_TYPE_STRING;
      }

      if (strcmp(funcName->val, "integer") == 0) {
        return INFER_TYPE_INT;
      }

      if (strcmp(funcName->val, "float") == 0) {
        return INFER_TYPE_FLOAT;
      }

      if (strcmp(funcName->val, "string") == 0) {
        return INFER_TYPE_STRING;
      }

      //  Comparison operators return INT (boolean 0/1)
      if (strcmp(funcName->val, "is") == 0 ||
          strcmp(funcName->val, "less_than") == 0 ||
          strcmp(funcName->val, "greater_than") == 0) {
        return INFER_TYPE_INT;
      }

      //  Logical operators return INT (boolean 0/1)
      if (strcmp(funcName->val, "not") == 0 ||
          strcmp(funcName->val, "and") == 0 ||
          strcmp(funcName->val, "or") == 0) {
        return INFER_TYPE_INT;
      }

      //  Math functions returning INT
      if (strcmp(funcName->val, "floor") == 0 ||
          strcmp(funcName->val, "ceil") == 0 ||
          strcmp(funcName->val, "round") == 0 ||
          strcmp(funcName->val, "abs") == 0 ||
          strcmp(funcName->val, "min") == 0 ||
          strcmp(funcName->val, "max") == 0 ||
          strcmp(funcName->val, "remainder") == 0 ||
          strcmp(funcName->val, "random_int") == 0) {
        // Check if arguments contain floats
        int hasFloat = 0;
        for (int i = 1; i < node->childCount; i++) {
          InferredTypeKind argType = inferExpressionType(node->children[i], functionNode);
          if (argType == INFER_TYPE_FLOAT) {
            hasFloat = 1;
            break;
          }
        }
        // abs/min/max can return float if input is float
        if (hasFloat && (strcmp(funcName->val, "abs") == 0 ||
                         strcmp(funcName->val, "min") == 0 ||
                         strcmp(funcName->val, "max") == 0)) {
          return INFER_TYPE_FLOAT;
        }
        return INFER_TYPE_INT;
      }

      //  Math functions returning FLOAT
      if (strcmp(funcName->val, "power") == 0 ||
          strcmp(funcName->val, "sqrt") == 0 ||
          strcmp(funcName->val, "random") == 0 ||
          strcmp(funcName->val, "random_range") == 0) {
        return INFER_TYPE_FLOAT;
      }

      return INFER_TYPE_UNKNOWN;
    }

    case OP_STATEMENT: {
      // Statement: infer type of last child
      if (node->childCount > 0) {
        return inferExpressionType(node->children[node->childCount - 1], functionNode);
      }
      return INFER_TYPE_UNKNOWN;
    }

    case OP_RETURN: {
      // Return: infer type of return value
      if (node->childCount > 0) {
        return inferExpressionType(node->children[0], functionNode);
      }
      return INFER_TYPE_VOID;
    }

    default:
      return INFER_TYPE_UNKNOWN;
  }
}

/**
 * Analyze parameter usage to infer parameter types
 * Strategy: Parameters inherit the type of the return value
 *
 *  Simple rule - if function returns float, parameters are float
 * This handles cases like: {x y -> <- (add x y)} called with floats
 */
static int TypeInfer_findParamIndex(AstNode *functionNode, const char *name) {
  if (!functionNode || !name) return -1;

  int paramIndex = 0;
  while (paramIndex < functionNode->childCount) {
    AstNode *paramNode = functionNode->children[paramIndex];
    if (!paramNode || paramNode->opcode != OP_IDENTIFIER) break;
    if (paramNode->val && strcmp(paramNode->val, name) == 0) {
      return paramIndex;
    }
    paramIndex++;
  }

  return -1;
}

static InferredTypeKind TypeInfer_findVariableTypeRecursive(AstNode *node,
                                                            const char *name,
                                                            AstNode *functionNode) {
  if (!node || !name) return INFER_TYPE_UNKNOWN;

  if (node->opcode == OP_ASSIGNMENT && node->childCount >= 2) {
    AstNode *target = node->children[0];
    if (target && target->opcode == OP_IDENTIFIER && target->val &&
        strcmp(target->val, name) == 0) {
      return inferExpressionType(node->children[1], functionNode);
    }
  }

  for (int i = 0; i < node->childCount; i++) {
    InferredTypeKind childType = TypeInfer_findVariableTypeRecursive(node->children[i], name, functionNode);
    if (childType != INFER_TYPE_UNKNOWN) {
      return childType;
    }
  }

  return INFER_TYPE_UNKNOWN;
}

static InferredTypeKind TypeInfer_findVariableType(AstNode *functionNode, const char *name) {
  if (!functionNode || !name) return INFER_TYPE_UNKNOWN;

  int childIndex = 0;
  while (childIndex < functionNode->childCount) {
    AstNode *child = functionNode->children[childIndex];
    if (!child || child->opcode != OP_IDENTIFIER) break;
    childIndex++;
  }

  for (; childIndex < functionNode->childCount; childIndex++) {
    InferredTypeKind found = TypeInfer_findVariableTypeRecursive(functionNode->children[childIndex],
                                                                 name, functionNode);
    if (found != INFER_TYPE_UNKNOWN) {
      return found;
    }
  }

  return INFER_TYPE_UNKNOWN;
}

static void TypeInfer_refineParamTypesFromUsage(AstNode *node,
                                                AstNode *functionNode,
                                                InferredFunctionType *result) {
  if (!node || !functionNode || !result || !result->paramTypes) return;

  if (node->opcode == OP_APPLICATION && node->childCount > 0) {
    AstNode *funcNode = node->children[0];
    if (funcNode && funcNode->opcode == OP_IDENTIFIER &&
        strcmp(funcNode->val, "is") == 0 && node->childCount >= 3) {
      for (int argIndex = 1; argIndex <= 2 && argIndex < node->childCount; argIndex++) {
        AstNode *argNode = node->children[argIndex];
        if (!argNode || argNode->opcode != OP_IDENTIFIER || !argNode->val) continue;

        int paramIndex = TypeInfer_findParamIndex(functionNode, argNode->val);
        if (paramIndex < 0 || paramIndex >= result->paramCount) continue;

        int otherIndex = (argIndex == 1) ? 2 : 1;
        if (otherIndex >= node->childCount) continue;

        InferredTypeKind otherKind = inferExpressionType(node->children[otherIndex], functionNode);
        if (otherKind == INFER_TYPE_UNKNOWN) {
          AstNode *otherNode = node->children[otherIndex];
          if (otherNode && otherNode->opcode == OP_IDENTIFIER && otherNode->val) {
            otherKind = TypeInfer_findVariableType(functionNode, otherNode->val);
          }
        }
        if (otherKind == INFER_TYPE_STRING) {
          result->paramTypes[paramIndex] = INFER_TYPE_STRING;
        } else if (otherKind == INFER_TYPE_FLOAT) {
          if (result->paramTypes[paramIndex] == INFER_TYPE_INT ||
              result->paramTypes[paramIndex] == INFER_TYPE_UNKNOWN) {
            result->paramTypes[paramIndex] = INFER_TYPE_FLOAT;
          }
        } else if (otherKind == INFER_TYPE_INT) {
          if (result->paramTypes[paramIndex] == INFER_TYPE_UNKNOWN) {
            result->paramTypes[paramIndex] = INFER_TYPE_INT;
          }
        }
      }
    }
  }

  for (int i = 0; i < node->childCount; i++) {
    TypeInfer_refineParamTypesFromUsage(node->children[i], functionNode, result);
  }
}

static void inferParameterTypes(AstNode *bodyNode, InferredFunctionType *result, AstNode *functionNode) {
  if (!bodyNode || !result) return;

  //  Simple heuristic
  // If the function returns a float, assume all parameters are float
  // If the function returns a string, assume all parameters are string
  // Otherwise, default to int
  //
  // This is a simplification but works for monomorphic functions
  //  will add proper constraint-based inference

  InferredTypeKind paramType = INFER_TYPE_INT;  // Default

  // If return type is float, assume parameters are float
  if (result->returnType == INFER_TYPE_FLOAT) {
    paramType = INFER_TYPE_FLOAT;
  } else if (result->returnType == INFER_TYPE_STRING) {
    paramType = INFER_TYPE_STRING;
  }

  // Apply the same type to all parameters
  for (int paramIndex = 0; paramIndex < result->paramCount; paramIndex++) {
    result->paramTypes[paramIndex] = paramType;
  }

  TypeInfer_refineParamTypesFromUsage(bodyNode, functionNode, result);
}

// ============================================================================
// Public API Implementation
// ============================================================================

InferredFunctionType *TypeInfer_inferFunction(AstNode *functionNode) {
  if (!functionNode || functionNode->opcode != OP_FUNCTION) {
    fprintf(stderr, "ERROR: TypeInfer_inferFunction called with non-function node\n");
    return NULL;
  }

  if (functionNode->childCount == 0) {
    fprintf(stderr, "ERROR: TypeInfer_inferFunction called with empty function\n");
    return NULL;
  }

  // Allocate result
  InferredFunctionType *result = (InferredFunctionType *)malloc(sizeof(InferredFunctionType));
  if (!result) {
    fprintf(stderr, "ERROR: Failed to allocate InferredFunctionType\n");
    return NULL;
  }

  // Extract parameters and body
  result->paramCount = functionNode->childCount - 1;
  result->paramTypes = (InferredTypeKind *)malloc(result->paramCount * sizeof(InferredTypeKind));
  result->functionName = strdup("_lambda");  // Anonymous function
  result->lineNumber = functionNode->lineNumber;

  if (!result->paramTypes && result->paramCount > 0) {
    fprintf(stderr, "ERROR: Failed to allocate parameter types\n");
    free(result);
    return NULL;
  }

  // Initialize all parameter types to UNKNOWN
  for (int i = 0; i < result->paramCount; i++) {
    result->paramTypes[i] = INFER_TYPE_UNKNOWN;
  }

  // Get function body
  AstNode *bodyNode = functionNode->children[result->paramCount];

  // Infer return type from body
  result->returnType = inferExpressionType(bodyNode, functionNode);

  //  Keep UNKNOWN types as-is - let LLVM codegen use actual runtime type
  // This enables polymorphic functions like {x y -> <- (add x y)}
  // Old behavior: defaulted UNKNOWN to INT (too restrictive)
  // New behavior: UNKNOWN → use actual body return type at compile time

  // Infer parameter types
  inferParameterTypes(bodyNode, result, functionNode);

  return result;
}

void TypeInfer_freeInferredType(InferredFunctionType *type) {
  if (!type) return;

  if (type->paramTypes) {
    free(type->paramTypes);
  }

  if (type->functionName) {
    free(type->functionName);
  }

  free(type);
}
