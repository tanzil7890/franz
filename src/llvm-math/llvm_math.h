#ifndef LLVM_MATH_H
#define LLVM_MATH_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

//  Enhanced Math Functions for LLVM
// Implements remainder, power, random, floor, ceil, round, abs, min, max, sqrt

// Remainder/modulo operations
LLVMValueRef LLVMCodeGen_compileRemainder(LLVMCodeGen *gen, AstNode *node);

// Power (exponentiation)
LLVMValueRef LLVMCodeGen_compilePower(LLVMCodeGen *gen, AstNode *node);

// Random number generation
LLVMValueRef LLVMCodeGen_compileRandom(LLVMCodeGen *gen, AstNode *node);

//  Extended random functions
LLVMValueRef LLVMCodeGen_compileRandomInt(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileRandomRange(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileRandomSeed(LLVMCodeGen *gen, AstNode *node);

// Rounding functions
LLVMValueRef LLVMCodeGen_compileFloor(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileCeil(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileRound(LLVMCodeGen *gen, AstNode *node);

// Absolute value
LLVMValueRef LLVMCodeGen_compileAbs(LLVMCodeGen *gen, AstNode *node);

// Min/Max (variadic)
LLVMValueRef LLVMCodeGen_compileMin(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileMax(LLVMCodeGen *gen, AstNode *node);

// Square root
LLVMValueRef LLVMCodeGen_compileSqrt(LLVMCodeGen *gen, AstNode *node);

#endif
