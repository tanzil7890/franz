#include "llvm_file_advanced.h"
#include "../file-advanced/file_advanced.h"
#include <stdio.h>

/**
 *  LLVM Advanced File Operations Implementation
 *
 * Industry-standard LLVM compilation following Rust's std::fs architecture.
 * Direct C function calls from LLVM IR for maximum performance.
 */

// Helper: Ensure runtime functions are declared in LLVM module
static void ensureRuntimeFileAdvancedFunctions(LLVMCodeGen *gen) {
  LLVMTypeRef i8PtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMTypeRef sizeTPtr = LLVMPointerType(gen->intType, 0);

  // char *readBinary(char *path, size_t *outSize)
  if (!LLVMGetNamedFunction(gen->module, "readBinary")) {
    LLVMTypeRef params[] = { i8PtrType, sizeTPtr };
    LLVMTypeRef funcType = LLVMFunctionType(i8PtrType, params, 2, 0);
    LLVMAddFunction(gen->module, "readBinary", funcType);
  }

  // void writeBinary(char *path, char *data, size_t size, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "writeBinary")) {
    LLVMTypeRef params[] = { i8PtrType, i8PtrType, gen->intType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(LLVMVoidTypeInContext(gen->context), params, 4, 0);
    LLVMAddFunction(gen->module, "writeBinary", funcType);
  }

  // Generic *franz_list_files(char *dirpath, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "franz_list_files")) {
    LLVMTypeRef params[] = { i8PtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(i8PtrType, params, 2, 0);  // Returns Generic* as i8*
    LLVMAddFunction(gen->module, "franz_list_files", funcType);
  }

  // int createDir(char *dirpath, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "createDir")) {
    LLVMTypeRef params[] = { i8PtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 2, 0);
    LLVMAddFunction(gen->module, "createDir", funcType);
  }

  // int dirExists(char *dirpath)
  if (!LLVMGetNamedFunction(gen->module, "dirExists")) {
    LLVMTypeRef params[] = { i8PtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "dirExists", funcType);
  }

  // int removeDir(char *dirpath, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "removeDir")) {
    LLVMTypeRef params[] = { i8PtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 2, 0);
    LLVMAddFunction(gen->module, "removeDir", funcType);
  }

  // long fileSize(char *path)
  if (!LLVMGetNamedFunction(gen->module, "fileSize")) {
    LLVMTypeRef params[] = { i8PtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "fileSize", funcType);
  }

  // long fileMtime(char *path)
  if (!LLVMGetNamedFunction(gen->module, "fileMtime")) {
    LLVMTypeRef params[] = { i8PtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "fileMtime", funcType);
  }

  // int isDirectory(char *path)
  if (!LLVMGetNamedFunction(gen->module, "isDirectory")) {
    LLVMTypeRef params[] = { i8PtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    LLVMAddFunction(gen->module, "isDirectory", funcType);
  }

  // Helper: Generic *Generic_new(enum Type type, void *p_val, int lineNumber)
  if (!LLVMGetNamedFunction(gen->module, "Generic_new")) {
    LLVMTypeRef params[] = { gen->intType, i8PtrType, gen->intType };
    LLVMTypeRef funcType = LLVMFunctionType(i8PtrType, params, 3, 0);
    LLVMAddFunction(gen->module, "Generic_new", funcType);
  }
}

LLVMValueRef LLVMFileAdvanced_compileReadBinary(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (read_binary filepath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: read_binary expects 1 argument (filepath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for read_binary at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Allocate size_t on stack for outSize
  LLVMValueRef sizePtr = LLVMBuildAlloca(gen->builder, gen->intType, "size_ptr");

  // Call readBinary(filepath, &size)
  LLVMValueRef readBinaryFunc = LLVMGetNamedFunction(gen->module, "readBinary");
  LLVMValueRef args[] = { filepath, sizePtr };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(readBinaryFunc),
                        readBinaryFunc, args, 2, "binary_data");
}

LLVMValueRef LLVMFileAdvanced_compileWriteBinary(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 2 arguments: (write_binary filepath data)
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: write_binary expects 2 arguments (filepath, data), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile arguments
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  LLVMValueRef data = LLVMCodeGen_compileNode(gen, node->children[1]);

  if (!filepath || !data) {
    fprintf(stderr, "ERROR: Failed to compile arguments for write_binary at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Get strlen(data) for size
  LLVMValueRef strlenFunc = LLVMGetNamedFunction(gen->module, "strlen");
  if (!strlenFunc) {
    LLVMTypeRef i8PtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    LLVMTypeRef params[] = { i8PtrType };
    LLVMTypeRef funcType = LLVMFunctionType(gen->intType, params, 1, 0);
    strlenFunc = LLVMAddFunction(gen->module, "strlen", funcType);
  }

  LLVMValueRef dataSize = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(strlenFunc),
                                          strlenFunc, &data, 1, "data_size");

  // Call writeBinary(filepath, data, size, lineNumber)
  LLVMValueRef writeBinaryFunc = LLVMGetNamedFunction(gen->module, "writeBinary");
  LLVMValueRef args[] = {
    filepath,
    data,
    dataSize,
    LLVMConstInt(gen->intType, node->lineNumber, 0)
  };

  LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(writeBinaryFunc),
                 writeBinaryFunc, args, 4, "");

  // Return void (0)
  return LLVMConstInt(gen->intType, 0, 0);
}

LLVMValueRef LLVMFileAdvanced_compileListFiles(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (list_files dirpath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: list_files expects 1 argument (dirpath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile dirpath argument
  LLVMValueRef dirpath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!dirpath) {
    fprintf(stderr, "ERROR: Failed to compile dirpath argument for list_files at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call franz_list_files(dirpath, lineNumber) - returns Generic* directly
  LLVMValueRef franzListFilesFunc = LLVMGetNamedFunction(gen->module, "franz_list_files");
  LLVMValueRef args[] = {
    dirpath,
    LLVMConstInt(gen->intType, node->lineNumber, 0)
  };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(franzListFilesFunc),
                        franzListFilesFunc, args, 2, "file_list_generic");
}

LLVMValueRef LLVMFileAdvanced_compileCreateDir(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (create_dir dirpath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: create_dir expects 1 argument (dirpath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile dirpath argument
  LLVMValueRef dirpath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!dirpath) {
    fprintf(stderr, "ERROR: Failed to compile dirpath argument for create_dir at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call createDir(dirpath, lineNumber)
  LLVMValueRef createDirFunc = LLVMGetNamedFunction(gen->module, "createDir");
  LLVMValueRef args[] = {
    dirpath,
    LLVMConstInt(gen->intType, node->lineNumber, 0)
  };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(createDirFunc),
                        createDirFunc, args, 2, "create_dir_result");
}

LLVMValueRef LLVMFileAdvanced_compileDirExists(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (dir_exists dirpath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: dir_exists expects 1 argument (dirpath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile dirpath argument
  LLVMValueRef dirpath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!dirpath) {
    fprintf(stderr, "ERROR: Failed to compile dirpath argument for dir_exists at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call dirExists(dirpath)
  LLVMValueRef dirExistsFunc = LLVMGetNamedFunction(gen->module, "dirExists");
  LLVMValueRef args[] = { dirpath };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dirExistsFunc),
                        dirExistsFunc, args, 1, "dir_exists_result");
}

LLVMValueRef LLVMFileAdvanced_compileRemoveDir(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (remove_dir dirpath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: remove_dir expects 1 argument (dirpath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile dirpath argument
  LLVMValueRef dirpath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!dirpath) {
    fprintf(stderr, "ERROR: Failed to compile dirpath argument for remove_dir at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call removeDir(dirpath, lineNumber)
  LLVMValueRef removeDirFunc = LLVMGetNamedFunction(gen->module, "removeDir");
  LLVMValueRef args[] = {
    dirpath,
    LLVMConstInt(gen->intType, node->lineNumber, 0)
  };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(removeDirFunc),
                        removeDirFunc, args, 2, "remove_dir_result");
}

LLVMValueRef LLVMFileAdvanced_compileFileSize(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (file_size filepath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: file_size expects 1 argument (filepath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for file_size at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call fileSize(filepath)
  LLVMValueRef fileSizeFunc = LLVMGetNamedFunction(gen->module, "fileSize");
  LLVMValueRef args[] = { filepath };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(fileSizeFunc),
                        fileSizeFunc, args, 1, "file_size_result");
}

LLVMValueRef LLVMFileAdvanced_compileFileMtime(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (file_mtime filepath)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: file_mtime expects 1 argument (filepath), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile filepath argument
  LLVMValueRef filepath = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!filepath) {
    fprintf(stderr, "ERROR: Failed to compile filepath argument for file_mtime at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call fileMtime(filepath)
  LLVMValueRef fileMtimeFunc = LLVMGetNamedFunction(gen->module, "fileMtime");
  LLVMValueRef args[] = { filepath };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(fileMtimeFunc),
                        fileMtimeFunc, args, 1, "file_mtime_result");
}

LLVMValueRef LLVMFileAdvanced_compileIsDirectory(LLVMCodeGen *gen, AstNode *node) {
  ensureRuntimeFileAdvancedFunctions(gen);

  // Expect 1 argument: (is_directory path)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: is_directory expects 1 argument (path), got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Compile path argument
  LLVMValueRef path = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!path) {
    fprintf(stderr, "ERROR: Failed to compile path argument for is_directory at line %d\n",
            node->lineNumber);
    return NULL;
  }

  // Call isDirectory(path)
  LLVMValueRef isDirectoryFunc = LLVMGetNamedFunction(gen->module, "isDirectory");
  LLVMValueRef args[] = { path };

  return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(isDirectoryFunc),
                        isDirectoryFunc, args, 1, "is_directory_result");
}
