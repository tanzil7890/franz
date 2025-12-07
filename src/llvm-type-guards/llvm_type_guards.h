#ifndef LLVM_TYPE_GUARDS_H
#define LLVM_TYPE_GUARDS_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

//  Type Guard Functions for LLVM Native Compilation
// Implements runtime type checking: is_int, is_float, is_string, is_list, is_function

/**
 * Compile is_int type guard
 * Returns 1 if value is an integer, 0 otherwise
 * Syntax: (is_int value)
 * Returns: i64 (0 or 1)
 */
LLVMValueRef LLVMCodeGen_compileIsInt(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile is_float type guard
 * Returns 1 if value is a float, 0 otherwise
 * Syntax: (is_float value)
 * Returns: i64 (0 or 1)
 */
LLVMValueRef LLVMCodeGen_compileIsFloat(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile is_string type guard
 * Returns 1 if value is a string, 0 otherwise
 * Syntax: (is_string value)
 * Returns: i64 (0 or 1)
 */
LLVMValueRef LLVMCodeGen_compileIsString(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile is_list type guard
 * Returns 1 if value is a list, 0 otherwise
 * Syntax: (is_list value)
 * Returns: i64 (0 or 1)
 * Note: Lists not yet fully supported in LLVM mode
 */
LLVMValueRef LLVMCodeGen_compileIsList(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile is_function type guard
 * Returns 1 if value is a function, 0 otherwise
 * Syntax: (is_function value)
 * Returns: i64 (0 or 1)
 */
LLVMValueRef LLVMCodeGen_compileIsFunction(LLVMCodeGen *gen, AstNode *node);

#endif
