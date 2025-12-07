#ifndef LLVM_REDUCE_H
#define LLVM_REDUCE_H

#include <llvm-c/Core.h>
#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 * LLVM Reduce Implementation
 *
 * Compiles the reduce function to native code via LLVM.
 * reduce takes a list, a callback closure, and an initial accumulator,
 * returning a single reduced value.
 *
 * Syntax: (reduce list callback initial)
 *         (reduce list callback)  // initial defaults to void
 *
 * Callback signature: {acc item index -> new_acc}
 *
 * Example:
 *   numbers = [1, 2, 3, 4, 5]
 *   sum_fn = {acc item idx -> <- (add acc item)}
 *   total = (reduce numbers sum_fn 0)
 *   // Result: 15
 *
 * The callback receives: (accumulator, element, index) -> new_accumulator
 * Returns: Final accumulated value (same type as initial/callback return)
 */

/**
 * Compile reduce function call to LLVM IR
 *
 * @param gen LLVM code generator context
 * @param node AST node for reduce application
 * @return LLVMValueRef Generic* pointer to result value
 */
LLVMValueRef LLVMReduce_compileReduce(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_REDUCE_H
