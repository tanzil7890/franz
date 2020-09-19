#ifndef TYPE_INFER_H
#define TYPE_INFER_H

#include "../ast.h"
#include "../generic.h"
#include <llvm-c/Core.h>

//  Type Inference System for Float/String Functions
// Infers parameter and return types for user-defined functions

// ============================================================================
// Type Representation
// ============================================================================

// Type kind for inferred types
typedef enum {
  INFER_TYPE_INT,       // i64
  INFER_TYPE_FLOAT,     // double
  INFER_TYPE_STRING,    // i8*
  INFER_TYPE_VOID,      // void
  INFER_TYPE_UNKNOWN    // Not yet inferred
} InferredTypeKind;

// Inferred function signature
typedef struct {
  InferredTypeKind *paramTypes;  // Array of parameter types
  int paramCount;                // Number of parameters
  InferredTypeKind returnType;   // Return type
  char *functionName;            // For error messages
  int lineNumber;                // For error messages
} InferredFunctionType;

// ============================================================================
// Type Inference Context
// ============================================================================

// Type inference context for analyzing functions
typedef struct {
  AstNode *functionNode;         // The function being analyzed
  InferredFunctionType *result;  // Inferred signature
  int errorCount;                // Number of errors encountered
} TypeInferContext;

// ============================================================================
// Public API
// ============================================================================

/**
 * Infer the type signature of a function
 *
 * This analyzes:
 * 1. Function body to understand return type
 * 2. Parameter usage to understand parameter types
 *
 * @param functionNode The OP_FUNCTION AST node
 * @return Inferred function signature (caller must free)
 */
InferredFunctionType *TypeInfer_inferFunction(AstNode *functionNode);

/**
 * Free an inferred function type
 */
void TypeInfer_freeInferredType(InferredFunctionType *type);

/**
 * Get LLVM type from inferred type kind
 */
LLVMTypeRef TypeInfer_getLLVMType(InferredTypeKind kind, LLVMContextRef context);

/**
 * Convert enum Type (from generic.h) to InferredTypeKind
 */
InferredTypeKind TypeInfer_fromGenericType(enum Type genericType);

/**
 * Get string representation of inferred type (for error messages)
 */
const char *TypeInfer_typeToString(InferredTypeKind kind);

#endif
