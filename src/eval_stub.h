#ifndef EVAL_STUB_H
#define EVAL_STUB_H

#include "ast.h"
#include "scope.h"
#include "generic.h"

//  Stub for eval() - used by stdlib runtime functions
// This is a TEMPORARY stub until  LLVM compilation is complete
//
// In , these functions will be compiled to LLVM IR and executed natively
// For now, we return NULL and print an error message

static inline Generic *eval(AstNode *node, Scope *scope, int depth) {
  (void)node;
  (void)scope;
  (void)depth;

  fprintf(stderr, "ERROR: Runtime evaluation not yet implemented in LLVM-only mode ()\n");
  fprintf(stderr, "This will be available in  when basic LLVM IR generation is complete.\n");
  fprintf(stderr, "For now, only compiled code will work (not dynamic eval/use/load).\n");

  return NULL;
}

#endif
