#ifndef LLVM_FILE_OPS_H
#define LLVM_FILE_OPS_H

#include "../llvm-codegen/llvm_codegen.h"
#include <llvm-c/Core.h>

/**
 *  LLVM File Operations
 *
 * This module provides native LLVM compilation for file I/O operations.
 * Enables reading and writing files with C-level performance in LLVM mode.
 *
 * Operations:
 * - read_file: Read entire file contents as string
 * - write_file: Write string contents to file
 */

/**
 * Compile (read_file filepath)
 * Reads entire file and returns contents as string (i8*)
 * Returns NULL on failure (file not found, read error)
 *
 * @param gen LLVM code generator context
 * @param node AST node with 1 child (filepath string)
 * @return LLVMValueRef - i8* (string) or NULL on error
 */
LLVMValueRef LLVMFileOps_compileReadFile(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (write_file filepath contents)
 * Writes string contents to file
 * Returns void (0) on success, exits on failure
 *
 * @param gen LLVM code generator context
 * @param node AST node with 2 children (filepath, contents)
 * @return LLVMValueRef - i64 (0 for void)
 */
LLVMValueRef LLVMFileOps_compileWriteFile(LLVMCodeGen *gen, AstNode *node);

/**
 *  Compile (file_exists filepath)
 * Checks if file exists and is readable
 * Returns 1 if file exists, 0 otherwise
 *
 * @param gen LLVM code generator context
 * @param node AST node with 1 child (filepath string)
 * @return LLVMValueRef - i64 (0 or 1)
 */
LLVMValueRef LLVMFileOps_compileFileExists(LLVMCodeGen *gen, AstNode *node);

/**
 *  Compile (append_file filepath contents)
 * Appends string contents to end of file
 * Creates file if it doesn't exist
 * Returns void (0) on success, exits on failure
 *
 * @param gen LLVM code generator context
 * @param node AST node with 2 children (filepath, contents)
 * @return LLVMValueRef - i64 (0 for void)
 */
LLVMValueRef LLVMFileOps_compileAppendFile(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_FILE_OPS_H
