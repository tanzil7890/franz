#ifndef LLVM_LISTS_H
#define LLVM_LISTS_H

#include <llvm-c/Core.h>
#include "../ast.h"
#include "../llvm-codegen/llvm_codegen.h"

/**
 *  LLVM List Literal Support
 *
 * Franz lists are heterogeneous (can contain int, float, string, list, function).
 * Strategy: Compile elements, create array of Generic* pointers, call runtime.
 *
 * Supports:
 * - Empty lists: []
 * - Integer lists: [1, 2, 3]
 * - Mixed type lists: [1, "hello", 3.14]
 * - Nested lists: [[1, 2], [3, 4]]
 */

/**
 * Compiles a list literal node
 * @param gen LLVM code generator context
 * @param node AST node with OP_LIST opcode and children as elements
 * @return LLVM value representing the list (opaque i8* pointer)
 */
LLVMValueRef LLVMLists_compileList(LLVMCodeGen *gen, AstNode *node);

#endif
