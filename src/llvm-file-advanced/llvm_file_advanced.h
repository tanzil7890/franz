#ifndef LLVM_FILE_ADVANCED_H
#define LLVM_FILE_ADVANCED_H

#include "../llvm-codegen/llvm_codegen.h"
#include <llvm-c/Core.h>

/**
 *  LLVM Advanced File Operations
 *
 * Industry-standard LLVM compilation for advanced file operations.
 * Follows Rust std::fs architecture with direct system call integration.
 *
 * Operations:
 * - Binary I/O: read_binary, write_binary
 * - Directory: list_files, create_dir, dir_exists, remove_dir
 * - Metadata: file_size, file_mtime, is_directory
 */

/**
 * Compile (read_binary filepath)
 * Reads raw binary data from file
 * @return LLVMValueRef - i8* (binary data as string)
 */
LLVMValueRef LLVMFileAdvanced_compileReadBinary(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (write_binary filepath data)
 * Writes raw binary data to file
 * @return LLVMValueRef - i64 (0 for void)
 */
LLVMValueRef LLVMFileAdvanced_compileWriteBinary(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (list_files dirpath)
 * Lists all files in directory
 * @return LLVMValueRef - Generic* (list of filenames)
 */
LLVMValueRef LLVMFileAdvanced_compileListFiles(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (create_dir dirpath)
 * Creates directory (mkdir -p behavior)
 * @return LLVMValueRef - i64 (1=success, 0=failure)
 */
LLVMValueRef LLVMFileAdvanced_compileCreateDir(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (dir_exists dirpath)
 * Checks if directory exists
 * @return LLVMValueRef - i64 (1=exists, 0=doesn't exist)
 */
LLVMValueRef LLVMFileAdvanced_compileDirExists(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (remove_dir dirpath)
 * Removes empty directory
 * @return LLVMValueRef - i64 (1=success, 0=failure)
 */
LLVMValueRef LLVMFileAdvanced_compileRemoveDir(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (file_size filepath)
 * Gets file size in bytes
 * @return LLVMValueRef - i64 (file size, -1 on error)
 */
LLVMValueRef LLVMFileAdvanced_compileFileSize(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (file_mtime filepath)
 * Gets file modification time (Unix timestamp)
 * @return LLVMValueRef - i64 (timestamp, -1 on error)
 */
LLVMValueRef LLVMFileAdvanced_compileFileMtime(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (is_directory path)
 * Checks if path is a directory
 * @return LLVMValueRef - i64 (1=is directory, 0=not directory)
 */
LLVMValueRef LLVMFileAdvanced_compileIsDirectory(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_FILE_ADVANCED_H
