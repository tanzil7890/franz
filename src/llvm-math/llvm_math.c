#include "llvm_math.h"
#include <stdio.h>
#include <stdlib.h>

//  Enhanced Math Functions for LLVM Native Compilation
// Implements: remainder, power, random, floor, ceil, round, abs, min, max, sqrt

// ============================================================================
// Helper: Type promotion for mixed int/float operations
// ============================================================================

static void promoteToFloat(LLVMCodeGen *gen, LLVMValueRef *value, LLVMTypeRef *type) {
  if (*type == gen->intType) {
    *value = LLVMBuildSIToFP(gen->builder, *value, gen->floatType, "itof");
    *type = gen->floatType;
  }
}

// ============================================================================
// remainder: (remainder a b) → a % b
// Returns: INT if both args are INT, FLOAT otherwise
// Uses: LLVM srem for integers, fmod() for floats
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRemainder(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: remainder requires exactly 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile both arguments using public API
  LLVMValueRef left = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef right = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!left || !right) return NULL;

  LLVMTypeRef leftType = LLVMTypeOf(left);
  LLVMTypeRef rightType = LLVMTypeOf(right);

  // Both integers: use srem (signed remainder)
  if (leftType == gen->intType && rightType == gen->intType) {
    return LLVMBuildSRem(gen->builder, left, right, "rem");
  }

  // Mixed or both floats: use fmod
  promoteToFloat(gen, &left, &leftType);
  promoteToFloat(gen, &right, &rightType);

  LLVMValueRef args[] = {left, right};
  return LLVMBuildCall2(gen->builder, gen->fmodType, gen->fmodFunc, args, 2, "fmod");
}

// ============================================================================
// power: (power base exponent) → base^exponent
// Returns: INT if both args are INT, FLOAT otherwise
// Uses: pow() from math.h
// ============================================================================

LLVMValueRef LLVMCodeGen_compilePower(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: power requires exactly 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile both arguments
  LLVMValueRef base = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef exponent = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!base || !exponent) return NULL;

  LLVMTypeRef baseType = LLVMTypeOf(base);
  LLVMTypeRef expType = LLVMTypeOf(exponent);

  int bothInts = (baseType == gen->intType && expType == gen->intType);

  // Convert to floats for pow()
  promoteToFloat(gen, &base, &baseType);
  promoteToFloat(gen, &exponent, &expType);

  // Call pow(base, exponent)
  LLVMValueRef args[] = {base, exponent};
  LLVMValueRef result = LLVMBuildCall2(gen->builder, gen->powType, gen->powFunc, args, 2, "pow");

  // Convert back to int if both inputs were integers
  if (bothInts) {
    result = LLVMBuildFPToSI(gen->builder, result, gen->intType, "ftoi");
  }

  return result;
}

// ============================================================================
// random: (random) → float in [0.0, 1.0)
// Returns: FLOAT
// Uses: rand() / RAND_MAX
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRandom(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 0) {
    fprintf(stderr, "ERROR: random requires 0 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Call rand()
  LLVMValueRef randVal = LLVMBuildCall2(gen->builder, gen->randType, gen->randFunc, NULL, 0, "rand");

  // Convert rand() result (i32) to double
  LLVMValueRef randDouble = LLVMBuildSIToFP(gen->builder, randVal, gen->floatType, "rand_float");

  // RAND_MAX constant (typically 2147483647)
  LLVMValueRef randMax = LLVMConstReal(gen->floatType, 2147483647.0);

  // Divide: rand() / RAND_MAX
  return LLVMBuildFDiv(gen->builder, randDouble, randMax, "random_result");
}

// ============================================================================
// random_int: (random_int n) → random integer in [0, n)
// Returns: INT
// Uses: rand() % n
// Example: (random_int 6) → 0-5 (dice roll needs +1)
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRandomInt(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: random_int requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile the max value argument
  LLVMValueRef maxVal = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!maxVal) return NULL;

  LLVMTypeRef maxType = LLVMTypeOf(maxVal);

  // Convert to int if it's a float
  if (maxType == gen->floatType) {
    maxVal = LLVMBuildFPToSI(gen->builder, maxVal, gen->intType, "ftoi");
    maxType = gen->intType;
  }

  // Runtime check: maxVal must be positive
  // We'll let it run and modulo will handle it (runtime behavior matches stdlib)

  // Call rand() -> i32
  LLVMValueRef randVal = LLVMBuildCall2(gen->builder, gen->randType, gen->randFunc, NULL, 0, "rand");

  // Convert rand() result to i64 for consistency
  LLVMValueRef randVal64 = LLVMBuildSExt(gen->builder, randVal, gen->intType, "rand64");

  // Compute: rand() % maxVal (signed remainder)
  LLVMValueRef result = LLVMBuildSRem(gen->builder, randVal64, maxVal, "random_int_result");

  // Handle negative remainder (if rand() was negative, which shouldn't happen, but be safe)
  // If result < 0, add maxVal to make it positive
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMValueRef isNegative = LLVMBuildICmp(gen->builder, LLVMIntSLT, result, zero, "is_neg");
  LLVMValueRef corrected = LLVMBuildAdd(gen->builder, result, maxVal, "corrected");
  result = LLVMBuildSelect(gen->builder, isNegative, corrected, result, "final_result");

  return result;
}

// ============================================================================
// random_range: (random_range min max) → random float in [min, max)
// Returns: FLOAT
// Uses: min + (rand() / RAND_MAX) * (max - min)
// Example: (random_range -10 40) → -10.0 to 39.999...
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRandomRange(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: random_range requires exactly 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile both arguments
  LLVMValueRef minVal = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef maxVal = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!minVal || !maxVal) return NULL;

  LLVMTypeRef minType = LLVMTypeOf(minVal);
  LLVMTypeRef maxType = LLVMTypeOf(maxVal);

  // Promote both to float
  promoteToFloat(gen, &minVal, &minType);
  promoteToFloat(gen, &maxVal, &maxType);

  // Call rand() -> i32
  LLVMValueRef randVal = LLVMBuildCall2(gen->builder, gen->randType, gen->randFunc, NULL, 0, "rand");

  // Convert rand() to double
  LLVMValueRef randDouble = LLVMBuildSIToFP(gen->builder, randVal, gen->floatType, "rand_float");

  // RAND_MAX constant
  LLVMValueRef randMax = LLVMConstReal(gen->floatType, 2147483647.0);

  // normalized = rand() / RAND_MAX (0.0 to 1.0)
  LLVMValueRef normalized = LLVMBuildFDiv(gen->builder, randDouble, randMax, "normalized");

  // range = max - min
  LLVMValueRef range = LLVMBuildFSub(gen->builder, maxVal, minVal, "range");

  // scaled = normalized * range
  LLVMValueRef scaled = LLVMBuildFMul(gen->builder, normalized, range, "scaled");

  // result = min + scaled
  LLVMValueRef result = LLVMBuildFAdd(gen->builder, minVal, scaled, "random_range_result");

  return result;
}

// ============================================================================
// random_seed: (random_seed n) → void
// Returns: VOID (conceptually - LLVM returns void, we return a dummy value)
// Uses: srand(n) to seed the RNG
// Example: (random_seed 42) → allows reproducible random sequences
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRandomSeed(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: random_seed requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile the seed argument
  LLVMValueRef seed = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!seed) return NULL;

  LLVMTypeRef seedType = LLVMTypeOf(seed);

  // Convert to int if it's a float
  if (seedType == gen->floatType) {
    seed = LLVMBuildFPToSI(gen->builder, seed, gen->intType, "ftoi");
  }

  // srand expects i32, so truncate i64 to i32
  LLVMValueRef seed32 = LLVMBuildTrunc(gen->builder, seed, LLVMInt32TypeInContext(gen->context), "seed32");

  // Call srand(seed32) - void functions must not have a name in LLVM
  LLVMValueRef args[] = {seed32};
  LLVMBuildCall2(gen->builder, gen->srandType, gen->srandFunc, args, 1, "");

  // random_seed returns void in Franz, but LLVM needs a value
  // Return 0 as a dummy value (following Franz TYPE_VOID convention)
  return LLVMConstInt(gen->intType, 0, 0);
}

// ============================================================================
// floor: (floor n) → largest integer <= n
// Returns: INT
// Uses: floor() from math.h
// ============================================================================

LLVMValueRef LLVMCodeGen_compileFloor(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: floor requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!value) return NULL;

  LLVMTypeRef valueType = LLVMTypeOf(value);

  // If already an integer, return as-is
  if (valueType == gen->intType) {
    return value;
  }

  // Call floor(value)
  LLVMValueRef args[] = {value};
  LLVMValueRef floored = LLVMBuildCall2(gen->builder, gen->floorType, gen->floorFunc, args, 1, "floor");

  // Convert double to int
  return LLVMBuildFPToSI(gen->builder, floored, gen->intType, "floor_int");
}

// ============================================================================
// ceil: (ceil n) → smallest integer >= n
// Returns: INT
// Uses: ceil() from math.h
// ============================================================================

LLVMValueRef LLVMCodeGen_compileCeil(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: ceil requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!value) return NULL;

  LLVMTypeRef valueType = LLVMTypeOf(value);

  // If already an integer, return as-is
  if (valueType == gen->intType) {
    return value;
  }

  // Call ceil(value)
  LLVMValueRef args[] = {value};
  LLVMValueRef ceiled = LLVMBuildCall2(gen->builder, gen->ceilType, gen->ceilFunc, args, 1, "ceil");

  // Convert double to int
  return LLVMBuildFPToSI(gen->builder, ceiled, gen->intType, "ceil_int");
}

// ============================================================================
// round: (round n) → nearest integer to n
// Returns: INT
// Uses: round() from math.h
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRound(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: round requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!value) return NULL;

  LLVMTypeRef valueType = LLVMTypeOf(value);

  // If already an integer, return as-is
  if (valueType == gen->intType) {
    return value;
  }

  // Call round(value)
  LLVMValueRef args[] = {value};
  LLVMValueRef rounded = LLVMBuildCall2(gen->builder, gen->roundType, gen->roundFunc, args, 1, "round");

  // Convert double to int
  return LLVMBuildFPToSI(gen->builder, rounded, gen->intType, "round_int");
}

// ============================================================================
// abs: (abs n) → |n|
// Returns: Same type as input (INT → INT, FLOAT → FLOAT)
// Uses: Conditional negation for INT, fabs() for FLOAT
// ============================================================================

LLVMValueRef LLVMCodeGen_compileAbs(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: abs requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!value) return NULL;

  LLVMTypeRef valueType = LLVMTypeOf(value);

  if (valueType == gen->intType) {
    // Integer: val < 0 ? -val : val
    LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
    LLVMValueRef isNeg = LLVMBuildICmp(gen->builder, LLVMIntSLT, value, zero, "is_neg");
    LLVMValueRef negated = LLVMBuildNeg(gen->builder, value, "neg");
    return LLVMBuildSelect(gen->builder, isNeg, negated, value, "abs");
  } else {
    // Float: use fabs()
    LLVMValueRef args[] = {value};
    return LLVMBuildCall2(gen->builder, gen->fabsType, gen->fabsFunc, args, 1, "fabs");
  }
}

// ============================================================================
// min: (min a b ...) → minimum value (variadic)
// Returns: INT if all args are INT, FLOAT otherwise
// Uses: Iterative comparison
// ============================================================================

LLVMValueRef LLVMCodeGen_compileMin(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 1) {
    fprintf(stderr, "ERROR: min requires at least 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile all arguments and check types
  LLVMValueRef *values = malloc(node->childCount * sizeof(LLVMValueRef));
  int hasFloat = 0;

  for (int i = 0; i < node->childCount; i++) {
    values[i] = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!values[i]) {
      free(values);
      return NULL;
    }
    if (LLVMTypeOf(values[i]) == gen->floatType) {
      hasFloat = 1;
    }
  }

  // If any float, promote all to float
  if (hasFloat) {
    for (int i = 0; i < node->childCount; i++) {
      LLVMTypeRef type = LLVMTypeOf(values[i]);
      promoteToFloat(gen, &values[i], &type);
    }
  }

  // Start with first value
  LLVMValueRef minVal = values[0];

  // Compare with rest
  for (int i = 1; i < node->childCount; i++) {
    LLVMValueRef cmp;
    if (hasFloat) {
      cmp = LLVMBuildFCmp(gen->builder, LLVMRealOLT, values[i], minVal, "fcmp_lt");
    } else {
      cmp = LLVMBuildICmp(gen->builder, LLVMIntSLT, values[i], minVal, "icmp_lt");
    }
    minVal = LLVMBuildSelect(gen->builder, cmp, values[i], minVal, "min_select");
  }

  free(values);
  return minVal;
}

// ============================================================================
// max: (max a b ...) → maximum value (variadic)
// Returns: INT if all args are INT, FLOAT otherwise
// Uses: Iterative comparison
// ============================================================================

LLVMValueRef LLVMCodeGen_compileMax(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 1) {
    fprintf(stderr, "ERROR: max requires at least 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile all arguments and check types
  LLVMValueRef *values = malloc(node->childCount * sizeof(LLVMValueRef));
  int hasFloat = 0;

  for (int i = 0; i < node->childCount; i++) {
    values[i] = LLVMCodeGen_compileNode(gen, node->children[i]);
    if (!values[i]) {
      free(values);
      return NULL;
    }
    if (LLVMTypeOf(values[i]) == gen->floatType) {
      hasFloat = 1;
    }
  }

  // If any float, promote all to float
  if (hasFloat) {
    for (int i = 0; i < node->childCount; i++) {
      LLVMTypeRef type = LLVMTypeOf(values[i]);
      promoteToFloat(gen, &values[i], &type);
    }
  }

  // Start with first value
  LLVMValueRef maxVal = values[0];

  // Compare with rest
  for (int i = 1; i < node->childCount; i++) {
    LLVMValueRef cmp;
    if (hasFloat) {
      cmp = LLVMBuildFCmp(gen->builder, LLVMRealOGT, values[i], maxVal, "fcmp_gt");
    } else {
      cmp = LLVMBuildICmp(gen->builder, LLVMIntSGT, values[i], maxVal, "icmp_gt");
    }
    maxVal = LLVMBuildSelect(gen->builder, cmp, values[i], maxVal, "max_select");
  }

  free(values);
  return maxVal;
}

// ============================================================================
// sqrt: (sqrt n) → √n
// Returns: FLOAT
// Uses: sqrt() from math.h
// ============================================================================

LLVMValueRef LLVMCodeGen_compileSqrt(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: sqrt requires exactly 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef value = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!value) return NULL;

  LLVMTypeRef valueType = LLVMTypeOf(value);

  // Convert to float if needed
  promoteToFloat(gen, &value, &valueType);

  // Call sqrt(value)
  LLVMValueRef args[] = {value};
  return LLVMBuildCall2(gen->builder, gen->sqrtType, gen->sqrtFunc, args, 1, "sqrt");
}
