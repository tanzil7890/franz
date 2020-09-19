#include <stdio.h>
#include <stdlib.h>
#include "tokens.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "types.h"
#include "typeinfer.h"
#include "typecheck.h"
#include "assert_types.h"

int franz_assert_types(const char *code, long fileLength, int verbose) {
  //  Array-based lexing
  TokenArray *tokens = lex((char *)code, (int)fileLength);

  //  Array-based parsing
  AstNode *ast = parseProgram(tokens);

  // Free tokens after parsing
  TokenArray_free(tokens);

  // Type check
  InferContext *ctx = InferContext_new();
  typecheck_add_stdlib(ctx->env);
  (void) infer(ast, ctx, 1);

  if (ctx->error_count > 0) {
    printf("\n\u2717 Type check failed (%d error%s)\n", ctx->error_count, ctx->error_count == 1 ? "" : "s");
    InferContext_free(ctx);
    AstNode_free(ast);
    return 0;
  } else if (verbose) {
    printf("\n\u2713 Type check passed\n");
  }

  InferContext_free(ctx);
  AstNode_free(ast);
  return 1;
}

