#include "run.h"

#include <stdbool.h>
#include <stddef.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>  //  For access() function

#include "tokens.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "events.h"
#include "stdlib.h"
#include "file.h"
#include "scope.h"

//  LLVM native compilation (default and only mode)
#include "llvm-codegen/llvm_codegen.h"

void exitHandler() {
  exit(0);
}

int run(char *code, long length, int argc, char *argv[], bool debug, bool enable_tco) {
  if (debug) {
    printf("\nTOKENS\n");
  }

  /*  Lex with array-based tokens */
  TokenArray *tokens = lex(code, length);

  if (debug) {
    // print tokens
    TokenArray_print(tokens);
    printf("Token Count: %i\n", tokens->count);
    printf("\nAST\n");
  }

  /*  Parse with array-based tokens */
  AstNode *p_headAstNode = parseProgram(tokens);

  if (debug) {
    // print AST
    AstNode_print(p_headAstNode, 0);
    printf("\nLLVM NATIVE COMPILATION\n");
  }

  /*  LLVM native compilation (Rust-level performance) */
  initEvents();

  // cleanly handle exit events
  signal(SIGTERM, exitHandler);
  signal(SIGINT, exitHandler);

  // Create global scope with stdlib
  Scope *p_global = newGlobal(argc, argv);

  // Initialize LLVM code generator
  LLVMCodeGen *codegen = LLVMCodeGen_new("franz_module");
  if (!codegen) {
    fprintf(stderr, "ERROR: Failed to initialize LLVM code generator\n");
    AstNode_free(p_headAstNode);
    TokenArray_free(tokens);
    Scope_free(p_global);
    return 1;
  }

  codegen->debugMode = debug;
  codegen->enableTCO = enable_tco;  // TCO: Enabled by default, disabled with --no-tco flag

  if (debug) {
    printf("[DEBUG] Compiling AST to LLVM IR...\n");
    if (enable_tco) {
      printf("[DEBUG] Tail Call Optimization ENABLED (default)\n");
    } else {
      printf("[DEBUG] Tail Call Optimization DISABLED (--no-tco flag used)\n");
    }
    fflush(stdout);
  }

  // Compile AST to LLVM IR ( stub prints status)
  int compileResult = LLVMCodeGen_compile(codegen, p_headAstNode, p_global);

  if (compileResult != 0) {
    fprintf(stderr, "ERROR: LLVM compilation failed\n");
    LLVMCodeGen_free(codegen);
    AstNode_free(p_headAstNode);
    TokenArray_free(tokens);
    Scope_free(p_global);
    return 1;
  }

  if (debug) {
    printf("[DEBUG] Compilation complete\n");
    LLVMCodeGen_dumpIR(codegen);
    fflush(stdout);
  }

  //  Write LLVM IR to file and compile to native executable
  const char *llFilename = "/tmp/franz_output.ll";
  const char *objFilename = "/tmp/franz_output.o";
  const char *exeFilename = "/tmp/franz_output";

  if (debug) {
    printf("[DEBUG] Writing LLVM IR to %s\n", llFilename);
    fflush(stdout);
  }

  // Write LLVM IR to .ll file
  char *error = NULL;
  if (LLVMPrintModuleToFile(codegen->module, llFilename, &error)) {
    fprintf(stderr, "ERROR: Failed to write LLVM IR to file: %s\n", error);
    LLVMDisposeMessage(error);
    LLVMCodeGen_free(codegen);
    AstNode_free(p_headAstNode);
    TokenArray_free(tokens);
    Scope_free(p_global);
    return 1;
  }

  if (debug) {
    printf("[DEBUG] Compiling LLVM IR to object file...\n");
    fflush(stdout);
  }

  // Compile LLVM IR to object file using llc
  char llcCmd[512];
  snprintf(llcCmd, sizeof(llcCmd), "/opt/homebrew/opt/llvm@17/bin/llc -filetype=obj %s -o %s",
           llFilename, objFilename);
  int llcResult = system(llcCmd);

  if (llcResult != 0) {
    fprintf(stderr, "ERROR: Failed to compile LLVM IR to object file\n");
    LLVMCodeGen_free(codegen);
    AstNode_free(p_headAstNode);
    TokenArray_free(tokens);
    Scope_free(p_global);
    return 1;
  }

  if (debug) {
    printf("[DEBUG] Linking object file to executable...\n");
    fflush(stdout);
  }

  //  Compile runtime dependencies for runtime linking
  const char *numberParseC = "src/number-formats/number_parse.c";
  const char *numberParseObj = "/tmp/number_parse.o";

  char compileNumberCmd[512];
  snprintf(compileNumberCmd, sizeof(compileNumberCmd),
           "gcc -c %s -o %s 2>/dev/null", numberParseC, numberParseObj);
  system(compileNumberCmd);  // Compile number_parse.c to object file

  //  Compile terminal_runtime.c for runtime linking
  const char *terminalRuntimeC = "src/llvm-terminal/terminal_runtime.c";
  const char *terminalRuntimeObj = "/tmp/terminal_runtime.o";

  char compileTerminalCmd[512];
  snprintf(compileTerminalCmd, sizeof(compileTerminalCmd),
           "gcc -c %s -o %s 2>/dev/null", terminalRuntimeC, terminalRuntimeObj);
  system(compileTerminalCmd);  // Compile terminal_runtime.c to object file

  //  Compile llvm_repeat.c for runtime linking
  const char *repeatC = "src/llvm-terminal/llvm_repeat.c";
  const char *repeatObj = "/tmp/llvm_repeat.o";

  char compileRepeatCmd[512];
  snprintf(compileRepeatCmd, sizeof(compileRepeatCmd),
           "gcc -c %s -o %s 2>/dev/null", repeatC, repeatObj);
  system(compileRepeatCmd);  // Compile llvm_repeat.c to object file

  //  Compile dict.c for runtime linking (dictionary/hash map support)
  const char *dictC = "src/dict.c";
  const char *dictObj = "/tmp/dict.o";

  if (debug) {
    printf("[DEBUG] Compiling dict.c...\n");
    fflush(stdout);
  }

  char compileDictCmd[512];
  snprintf(compileDictCmd, sizeof(compileDictCmd),
           "gcc -c %s -o %s", dictC, dictObj);

  if (debug) {
    printf("[DEBUG] Dict compile command: %s\n", compileDictCmd);
    fflush(stdout);
  }

  int dictCompileResult = system(compileDictCmd);

  if (dictCompileResult != 0) {
    fprintf(stderr, "[ERROR] Failed to compile dict.c (exit code: %d)\n", dictCompileResult);
  } else if (debug) {
    printf("[DEBUG] Successfully compiled dict.c to %s\n", dictObj);
    // Verify symbols in dict.o
    system("nm /tmp/dict.o | grep -c Dict 2>/dev/null | xargs -I{} printf '[DEBUG] dict.o contains {} Dict symbols\\n'");
    fflush(stdout);
  }

  //  Compile stdlib.c for runtime linking (dict wrapper functions)
  const char *stdlibC = "src/stdlib.c";
  const char *stdlibObj = "/tmp/stdlib.o";

  if (debug) {
    printf("[DEBUG] Compiling stdlib.c...\n");
    fflush(stdout);
  }

  char compileStdlibCmd[512];
  snprintf(compileStdlibCmd, sizeof(compileStdlibCmd),
           "gcc -c %s -o %s", stdlibC, stdlibObj);

  if (debug) {
    printf("[DEBUG] Stdlib compile command: %s\n", compileStdlibCmd);
    fflush(stdout);
  }

  int stdlibCompileResult = system(compileStdlibCmd);

  if (stdlibCompileResult != 0) {
    fprintf(stderr, "[ERROR] Failed to compile stdlib.c (exit code: %d)\n", stdlibCompileResult);
  } else if (debug) {
    printf("[DEBUG] Successfully compiled stdlib.c to %s\n", stdlibObj);
    // Verify franz_dict_* symbols in stdlib.o
    system("nm /tmp/stdlib.o | grep -c franz_dict 2>/dev/null | xargs -I{} printf '[DEBUG] stdlib.o contains {} franz_dict symbols\\n'");
    fflush(stdout);
  }

  //  Build runtime library if not exists (industry-standard static library)
  const char *runtimeLib = "/tmp/libfranz_runtime.a";
  if (access(runtimeLib, F_OK) != 0) {
    // Runtime library doesn't exist - build it
    system("make -f Makefile.runtime 2>/dev/null");
  }

  // Link object file + runtime library to executable using clang
  //  Include dict.o and stdlib.o for dict runtime support
  char clangCmd[1024];
  snprintf(clangCmd, sizeof(clangCmd), "clang %s %s %s %s %s %s %s -lm -o %s",
           objFilename, numberParseObj, terminalRuntimeObj, repeatObj,
           dictObj, stdlibObj, runtimeLib, exeFilename);

  if (debug) {
    printf("[DEBUG] Linking command: %s\n", clangCmd);
    fflush(stdout);
  }

  int clangResult = system(clangCmd);

  if (clangResult != 0) {
    fprintf(stderr, "ERROR: Failed to link executable\n");
    LLVMCodeGen_free(codegen);
    AstNode_free(p_headAstNode);
    TokenArray_free(tokens);
    Scope_free(p_global);
    return 1;
  }

  if (debug) {
    printf("[DEBUG] Executing native binary: %s\n", exeFilename);
    fflush(stdout);
  }

  // Execute the native binary
  int execResult = system(exeFilename);
  int exitCode = WEXITSTATUS(execResult);

  /* free */
  if (debug) {
    printf("[DEBUG] Starting cleanup...\n");
    fflush(stdout);
  }

  // free LLVM code generator
  if (debug) {
    printf("[DEBUG] Freeing LLVM codegen...\n");
    fflush(stdout);
  }
  LLVMCodeGen_free(codegen);
  codegen = NULL;
  if (debug) {
    printf("[DEBUG] LLVM codegen freed successfully\n");
    fflush(stdout);
    printf("LLVM CodeGen Freed\n");
  }

  //  Free token array
  // printf("[DEBUG] Freeing tokens...\n");
  // fflush(stdout);
  TokenArray_free(tokens);
  tokens = NULL;
  // printf("[DEBUG] Tokens freed successfully\n");
  // fflush(stdout);
  if (debug) printf("Tokens Freed\n");

  // free ast
  // printf("[DEBUG] Freeing AST...\n");
  // fflush(stdout);
  AstNode_free(p_headAstNode);
  p_headAstNode = NULL;
  // printf("[DEBUG] AST freed successfully\n");
  // fflush(stdout);
  if (debug) printf("AST Freed\n");

  // free global scope
  // printf("[DEBUG] Freeing global scope...\n");
  // fflush(stdout);
  Scope_free(p_global);
  p_global = NULL;
  // printf("[DEBUG] Global scope freed successfully\n");
  // fflush(stdout);
  if (debug) printf("Global Scope Freed\n");

  // free file cache
  // printf("[DEBUG] Freeing file cache...\n");
  // fflush(stdout);
  FileCache_free();
  // printf("[DEBUG] File cache freed successfully\n");
  // fflush(stdout);
  if (debug) printf("File Cache Freed\n");

  return exitCode;
}
