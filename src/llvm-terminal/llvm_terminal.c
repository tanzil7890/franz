#include "llvm_terminal.h"
#include <stdio.h>

/**
 * LLVM Terminal Functions Implementation
 *
 *  Terminal dimension queries
 *
 * Strategy: Call C runtime functions via LLVM IR
 * - Declare external C functions (franz_get_rows, franz_get_columns)
 * - Generate LLVM call instructions
 * - Return i64 terminal dimensions
 *
 * Runtime functions are implemented in src/llvm-terminal/terminal_runtime.c
 */

// ============================================================================
// (rows) - Get terminal height
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRows_impl(LLVMCodeGen *gen, AstNode *node) {
  // rows() takes no arguments
  if (node->childCount != 0) {
    fprintf(stderr, "ERROR: rows requires 0 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Compiling rows() function call\n");
  }

  // Verify runtime function is declared
  if (!gen->getTerminalRowsFunc) {
    fprintf(stderr, "ERROR: Terminal rows runtime function not declared\n");
    return NULL;
  }

  // Generate call to franz_get_terminal_rows()
  // Returns i64 (terminal height)
  LLVMValueRef result = LLVMBuildCall2(gen->builder,
                                        gen->getTerminalRowsType,
                                        gen->getTerminalRowsFunc,
                                        NULL, 0, "rows_result");

  if (!result) {
    fprintf(stderr, "ERROR: Failed to build call to franz_get_terminal_rows\n");
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] rows() call generated successfully\n");
  }

  return result;
}

// ============================================================================
// (columns) - Get terminal width
// ============================================================================

LLVMValueRef LLVMCodeGen_compileColumns_impl(LLVMCodeGen *gen, AstNode *node) {
  // columns() takes no arguments
  if (node->childCount != 0) {
    fprintf(stderr, "ERROR: columns requires 0 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Compiling columns() function call\n");
  }

  // Verify runtime function is declared
  if (!gen->getTerminalColumnsFunc) {
    fprintf(stderr, "ERROR: Terminal columns runtime function not declared\n");
    return NULL;
  }

  // Generate call to franz_get_terminal_columns()
  // Returns i64 (terminal width)
  LLVMValueRef result = LLVMBuildCall2(gen->builder,
                                        gen->getTerminalColumnsType,
                                        gen->getTerminalColumnsFunc,
                                        NULL, 0, "columns_result");

  if (!result) {
    fprintf(stderr, "ERROR: Failed to build call to franz_get_terminal_columns\n");
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] columns() call generated successfully\n");
  }

  return result;
}

// ============================================================================
// (repeat string count) - Repeat a string N times
// ============================================================================

LLVMValueRef LLVMCodeGen_compileRepeat_impl(LLVMCodeGen *gen, AstNode *node) {
  // repeat() takes exactly 2 arguments
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: repeat requires 2 arguments (string, count) at line %d\n", node->lineNumber);
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Compiling repeat(string, count) function call\n");
  }

  // Compile first argument (string)
  LLVMValueRef strArg = LLVMCodeGen_compileNode(gen, node->children[0]);
  if (!strArg) {
    fprintf(stderr, "ERROR: Failed to compile string argument for repeat\n");
    return NULL;
  }

  // Compile second argument (count)
  LLVMValueRef countArg = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!countArg) {
    fprintf(stderr, "ERROR: Failed to compile count argument for repeat\n");
    return NULL;
  }

  // Verify runtime function is declared
  if (!gen->repeatStringFunc) {
    fprintf(stderr, "ERROR: Repeat string runtime function not declared\n");
    return NULL;
  }

  // Call franz_repeat_string(str, count)
  LLVMValueRef args[] = {strArg, countArg};
  LLVMValueRef result = LLVMBuildCall2(gen->builder,
                                        gen->repeatStringType,
                                        gen->repeatStringFunc,
                                        args, 2, "repeat_result");

  if (!result) {
    fprintf(stderr, "ERROR: Failed to build call to franz_repeat_string\n");
    return NULL;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] repeat() call generated successfully\n");
  }

  return result;
}
