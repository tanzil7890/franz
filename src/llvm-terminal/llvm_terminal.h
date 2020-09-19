#ifndef LLVM_TERMINAL_H
#define LLVM_TERMINAL_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 * LLVM Terminal Functions
 *
 *  Terminal dimension queries for responsive CLI applications
 *
 * Functions:
 * - rows(): Returns terminal height (number of lines)
 * - columns(): Returns terminal width (number of characters)
 *
 * Implementation uses ioctl(TIOCGWINSZ) system call via LLVM runtime functions
 */

/**
 * Compile (rows) function call
 * Returns the terminal height (number of rows/lines)
 *
 * @param gen LLVM code generator context
 * @param node AST node for the rows function call
 * @return LLVM value representing terminal height (i64)
 */
LLVMValueRef LLVMCodeGen_compileRows(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (columns) function call
 * Returns the terminal width (number of columns/characters)
 *
 * @param gen LLVM code generator context
 * @param node AST node for the columns function call
 * @return LLVM value representing terminal width (i64)
 */
LLVMValueRef LLVMCodeGen_compileColumns(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (repeat string count) function call
 * Returns the string repeated count times
 *
 * @param gen LLVM code generator context
 * @param node AST node for the repeat function call
 * @return LLVM value representing repeated string (ptr)
 */
LLVMValueRef LLVMCodeGen_compileRepeat(LLVMCodeGen *gen, AstNode *node);

// Internal macros for implementation consistency
#define LLVMCodeGen_compileRows_impl LLVMCodeGen_compileRows
#define LLVMCodeGen_compileColumns_impl LLVMCodeGen_compileColumns
#define LLVMCodeGen_compileRepeat_impl LLVMCodeGen_compileRepeat

#endif // LLVM_TERMINAL_H
