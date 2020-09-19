#ifndef LLVM_LIST_OPS_H
#define LLVM_LIST_OPS_H

#include <llvm-c/Core.h>
#include "../ast.h"
#include "../llvm-codegen/llvm_codegen.h"

/**
 *  Industry-Standard LLVM List Operations
 *
 * Rust-like implementation of functional list operations:
 * - head: Get first element (car)
 * - tail: Get rest of list (cdr)
 * - cons: Prepend element to list
 * - empty?: Check if list is empty
 * - length: Get list length
 * - nth: Get element at index
 *
 * All operations work with heterogeneous lists (like Rust's Vec<Box<dyn Any>>)
 */

/**
 * Compile (head list) - get first element
 * @param gen LLVM code generator context
 * @param node AST node with function application
 * @return LLVM value (Generic*)
 */
LLVMValueRef LLVMListOps_compileHead(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (tail list) - get rest of list
 * @param gen LLVM code generator context
 * @param node AST node with function application
 * @return LLVM value (Generic*)
 */
LLVMValueRef LLVMListOps_compileTail(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (cons elem list) - prepend element
 * @param gen LLVM code generator context
 * @param node AST node with function application
 * @return LLVM value (Generic*)
 */
LLVMValueRef LLVMListOps_compileCons(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (empty? list) - check if empty
 * @param gen LLVM code generator context
 * @param node AST node with function application
 * @return LLVM value (i64: 0 or 1)
 */
LLVMValueRef LLVMListOps_compileIsEmpty(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (length list) - get length
 * @param gen LLVM code generator context
 * @param node AST node with function application
 * @return LLVM value (i64)
 */
LLVMValueRef LLVMListOps_compileLength(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (nth list index) - get element at index
 * @param gen LLVM code generator context
 * @param node AST node with function application
 * @return LLVM value (Generic*)
 */
LLVMValueRef LLVMListOps_compileNth(LLVMCodeGen *gen, AstNode *node);

#endif
