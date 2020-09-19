#include "llvm_file_ops.h"
#include "../file.h"
#include <stdio.h>

/**
 *  LLVM File Operations Implementation
 *
 * Provides native LLVM compilation for file I/O with C-level performance.
 * Uses existing readFile/writeFile functions from file.c
 */

// Helper: Ensure runtime file functions are declared
static void ensureRuntimeFileFunctions(LLVMCodeGen *gen) {
  LLVMTypeRef i8PtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMTypeRef boolType = LLVMInt1TypeInContext(gen->context);

  // char *readFile(char *path, bool cache)
  if (!LLVMGetNamedFunction(gen->module, "readFile")) {
    LLVMTypeRef params[] = { i8PtrType, boolType };
    LLVMTypeRef funcType = LLVMFunctionType(i8PtrType, params, 2, 0);
    LLVMAddFunction(gen->module, "readFile", funcType);
  }

  // void writeFile(char *path, char *contents, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "writeFile")) {
    LLVMTypeRef params[] = { i8PtrType, i8PtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(LLVMVoidTypeInContext(gen->context), params, 3, 0);
    LLVMAddFunction(gen->module, "writeFile", funcType);
  }

  //  int fileExists(char *path)
  if (!LLVMGetNamedFunction(gen->module, "fileExists")) {
    LLVMTypeRef params[] = { i8PtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "fileExists", funcType);
  }

  //  void appendFile(char *path, char *contents, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "appendFile")) {
    LLVMTypeRef params[] = { i8PtrType, i8PtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(LLVMVoidTypeInContext(gen->context), params, 3, 0);
    LLVMAddFunction(gen->module, "appendFile", funcType);
  }
}

LLVMValueRef LLVMFileOps_compileReadFile(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileFunctions(gen);

  // Expect 1 argument: (read_file filepath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: read_file expects 1 argument (filepath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for read_file at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Verify filepath is a string (i8*)
  LLVMTypeRef filepathType = LLVMTypeOf(filepath);
  if (LLVMGetTypeKind(filepathType) != LLVMPointerTypeKind) {
    fprintf(stderr, "ERROR: read_file filepath must be a string at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call readFile(filepath, false) - cache=false for file operations
  LLVMValueRef readFileFunc = LLVMGetNamedFunction(gen->module, "readFile");
  LLVMValueRef args[] = {
    filepath,
    LLVMConstInt(LLVMInt1TypeInContext(gen->context), 0, 0)  // cache=false
  };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(readFileFunc),
                        readFileFunc, args, 2, "file_contents");
}

LLVMValueRef LLVMFileOps_compileWriteFile(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileFunctions(gen);

  // Expect 2 arguments: (write_file filepath contents)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: write_file expects 2 arguments (filepath, contents), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for write_file at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile contents argument
  LLVMValueRef contents = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!contents) {
    fprintf(stderr, "ERROR: Failed to compile contents argument for write_file at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Verify both are strings (i8*)
  LLVMTypeRef filepathType = LLVMTypeOf(filepath);
  LLVMTypeRef contentsType = LLVMTypeOf(contents);

  if (LLVMGetTypeKind(filepathType) != LLVMPointerTypeKind) {
    fprintf(stderr, "ERROR: write_file filepath must be a string at line %d\n",
            node->lineNumber);
    return NULL;
  }

  if (LLVMGetTypeKind(contentsType) != LLVMPointerTypeKind) {
    fprintf(stderr, "ERROR: write_file contents must be a string at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call writeFile(filepath, contents, lineNumber)
  LLVMValueRef writeFileFunc = LLVMGetNamedFunction(gen->module, "writeFile");
  LLVMValueRef args[] = {
    filepath,
    contents,
    LLVMConstInt(gen->intType, node->lineNumber, 0)  // lineNumber for error reporting
  };

  LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(writeFileFunc),
                 writeFileFunc, args, 3, "");

  // Return void (represented as 0 in Franz)
  return LLVMConstInt(gen->intType, 0, 0);
}

LLVMValueRef LLVMFileOps_compileFileExists(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileFunctions(gen);

  // Expect 1 argument: (file_exists filepath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: file_exists expects 1 argument (filepath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for file_exists at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Verify filepath is a string (i8*)
  LLVMTypeRef filepathType = LLVMTypeOf(filepath);
  if (LLVMGetTypeKind(filepathType) != LLVMPointerTypeKind) {
    fprintf(stderr, "ERROR: file_exists filepath must be a string at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call fileExists(filepath) - returns i64 (0 or 1)
  LLVMValueRef fileExistsFunc = LLVMGetNamedFunction(gen->module, "fileExists");
  LLVMValueRef args[] = { filepath };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(fileExistsFunc),
                        fileExistsFunc, args, 1, "file_exists_result");
}

LLVMValueRef LLVMFileOps_compileAppendFile(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileFunctions(gen);

  // Expect 2 arguments: (append_file filepath contents)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: append_file expects 2 arguments (filepath, contents), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for append_file at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Compile contents argument
  LLVMValueRef contents = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!contents) {
    fprintf(stderr, "ERROR: Failed to compile contents argument for append_file at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Verify both are strings (i8*)
  LLVMTypeRef filepathType = LLVMTypeOf(filepath);
  LLVMTypeRef contentsType = LLVMTypeOf(contents);

  if (LLVMGetTypeKind(filepathType) != LLVMPointerTypeKind) {
    fprintf(stderr, "ERROR: append_file filepath must be a string at line %d\n",
            node->lineNumber);
    return NULL;
  }

  if (LLVMGetTypeKind(contentsType) != LLVMPointerTypeKind) {
    fprintf(stderr, "ERROR: append_file contents must be a string at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call appendFile(filepath, contents, lineNumber)
  LLVMValueRef appendFileFunc = LLVMGetNamedFunction(gen->module, "appendFile");
  LLVMValueRef args[] = {
    filepath,
    contents,
    LLVMConstInt(gen->intType, node->lineNumber, 0)  // lineNumber for error reporting
  };

  LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(appendFileFunc),
                 appendFileFunc, args, 3, "");

  // Return void (represented as 0 in Franz)
  return LLVMConstInt(gen->intType, 0, 0);
}
