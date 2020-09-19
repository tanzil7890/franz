#ifndef LLVM_TYPE_H
#define LLVM_TYPE_H

#include <llvm-c/Core.h>
#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

//  LLVM Type Function Implementation
// Returns the type of a value as a string

/**
 * Compile the type() function call
 *
 * Syntax: (type value)
 *
 * Returns a string indicating the type of the value:
 * - "integer" for integers
 * - "float" for floating point numbers
 * - "string" for strings
 * - "list" for lists
 * - "function" for functions
 * - "closure" for closures
 * - "void" for void values
 * - "dict" for dictionaries
 *
 * @param gen - LLVM code generator context
 * @param node - AST node for type() function application
 * @return LLVMValueRef - Compiled LLVM value (i8* string pointer)
 */
LLVMValueRef LLVMType_compileType(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_TYPE_H
