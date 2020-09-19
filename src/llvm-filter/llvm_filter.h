#ifndef LLVM_FILTER_H
#define LLVM_FILTER_H

#include <llvm-c/Core.h>
#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 * LLVM Filter Implementation
 *
 * Compiles the filter function to native code via LLVM.
 * filter takes a list and a predicate closure, returning elements that pass the test.
 *
 * Syntax: (filter list predicate)
 *
 * Example:
 *   numbers = [1, 2, 3, 4, 5]
 *   is_even = {x i -> <- (is (remainder x 2) 0)}
 *   evens = (filter numbers is_even)
 *   // Result: [2, 4, 6]
 *
 * The predicate receives: (element, index) -> boolean
 * Returns: New list with elements where predicate returned truthy value
 */

/**
 * Compile filter function call to LLVM IR
 *
 * @param gen LLVM code generator context
 * @param node AST node for filter application
 * @return LLVMValueRef Generic* pointer to result list
 */
LLVMValueRef LLVMFilter_compileFilter(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_FILTER_H
