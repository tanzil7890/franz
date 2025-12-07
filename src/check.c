#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"
#include "parse.h"
#include "tokens.h"
#include "types.h"
#include "typeinfer.h"
#include "typecheck.h"
#include "ast.h"

#define FRANZ_CHECK_VERSION "v0.1.0"

void print_usage() {
  printf("Franz Type Checker %s\n", FRANZ_CHECK_VERSION);
  printf("Usage: franz-check [options] <file.franz>\n\n");
  printf("Options:\n");
  printf("  -v, --version     Show version\n");
  printf("  -h, --help        Show this help\n");
  printf("  --show-types      Show inferred types for all definitions\n");
  printf("  --verbose         Verbose output\n\n");
  printf("Examples:\n");
  printf("  franz-check myprogram.franz\n");
  printf("  franz-check --show-types factorial.franz\n");
}

int main(int argc, char *argv[]) {
  int show_types = 0;
  int verbose = 0;
  int strict = 0;
  char *filename = NULL;

  // Parse arguments
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      printf("franz-check %s\n", FRANZ_CHECK_VERSION);
      return 0;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_usage();
      return 0;
    } else if (strcmp(argv[i], "--show-types") == 0) {
      show_types = 1;
    } else if (strcmp(argv[i], "--verbose") == 0) {
      verbose = 1;
    } else if (strcmp(argv[i], "--strict") == 0) {
      strict = 1;
    } else if (argv[i][0] != '-') {
      filename = argv[i];
    } else {
      fprintf(stderr, "Unknown option: %s\n", argv[i]);
      print_usage();
      return 1;
    }
  }

  if (filename == NULL) {
    fprintf(stderr, "Error: No input file specified\n\n");
    print_usage();
    return 1;
  }

  // Read file
  if (verbose) {
    printf("Reading file: %s\n", filename);
  }

  FILE *file = fopen(filename, "r");
  if (file == NULL) {
    fprintf(stderr, "Error: Cannot open file '%s'\n", filename);
    return 1;
  }

  // Get file size
  fseek(file, 0, SEEK_END);
  long filesize = ftell(file);
  rewind(file);

  // Read file contents
  char *code = (char *) malloc(filesize + 1);
  if (code == NULL) {
    fprintf(stderr, "Error: Memory allocation failed\n");
    fclose(file);
    return 1;
  }

  size_t read_size = fread(code, 1, filesize, file);
  code[read_size] = '\0';
  fclose(file);

  if (verbose) {
    printf("File size: %ld bytes\n", filesize);
  }

  // Lex
  if (verbose) {
    printf("Lexing...\n");
  }

  //  Array-based lexing
  TokenArray *tokens = lex(code, filesize);

  if (verbose) {
    printf("Token count: %d\n", tokens->count);
  }

  //  Array-based parsing
  if (verbose) {
    printf("Parsing...\n");
  }

  AstNode *ast = parseProgram(tokens);

  if (ast == NULL) {
    fprintf(stderr, "Error: Parsing failed\n");
    free(code);
    return 1;
  }

  if (verbose) {
    printf("Parsing complete\n");
  }

  // Create type environment with stdlib
  if (verbose) {
    printf("Initializing type environment...\n");
  }

  InferContext *ctx = InferContext_new();
  typecheck_add_stdlib(ctx->env);

  if (verbose) {
    printf("Standard library signatures loaded\n");
  }

  // Run type inference
  if (verbose) {
    printf("Running type inference...\n");
  }

  Type *result_type = infer(ast, ctx, 1);

  if (result_type == NULL) {
    fprintf(stderr, "\n✗ Type checking failed\n");
    InferContext_free(ctx);
    AstNode_free(ast);
    free(code);
    return 1;
  }

  // Report summary and decide exit code
  if (ctx->error_count > 0) {
    printf("\n✗ Type check failed (%d error%s)\n", ctx->error_count, ctx->error_count == 1 ? "" : "s");
  } else {
    printf("\n✓ Type check passed\n");
  }

  if ((show_types || verbose) && ctx->error_count == 0) {
    printf("\nProgram type: ");
    Type_print(Substitution_apply(ctx, result_type));
    printf("\n");
  }

  if (show_types) {
    printf("\nInferred types:\n");
    TypeEnv_print(ctx->env);
  }

  // Decide exit code before cleanup
  int exit_code = (strict && ctx->error_count > 0) ? 1 : 0;

  // Cleanup
  Type_free(result_type);
  InferContext_free(ctx);
  AstNode_free(ast);
  free(code);

  return exit_code;
}
