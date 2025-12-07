#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <signal.h>
#include <string.h>
#include "tokens.h"
#include "lex.h"
#include "ast.h"
#include "parse.h"
#include "generic.h"
#include "scope.h"
#include "stdlib.h"
#include "events.h"
#include "file.h"
#include "run.h"
#include "module_cache.h"
#include "circular-deps/circular_deps.h"
#include "error-handling/error_handler.h"
// Type checking (optional pre-run assertions)
#include "assert_types.h"

#define FRANZ_VERSION ("v0.0.4")

int main(int argc, char *argv[]) {
  // Initialize error handling system
  ErrorState_init();

  // Initialize module cache at startup
  ModuleCache_init();

  // Initialize circular dependency detection
  CircularDeps_init();

  //  Check environment variable for default scoping mode
  const char* env_scoping = getenv("FRANZ_SCOPING");
  if (env_scoping != NULL) {
    if (strcmp(env_scoping, "lexical") == 0) {
      g_scoping_mode = SCOPING_LEXICAL;
    } else if (strcmp(env_scoping, "dynamic") == 0) {
      g_scoping_mode = SCOPING_DYNAMIC;
    }
  }

  // parse flags: -v, -d, --assert-types, --scoping, --no-tco
  bool debug = false;
  bool assert_types = false;
  bool enable_tco = true;  // TCO: Enabled by default (functional language standard), use --no-tco to disable
  int first_arg_index = 1;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      printf("%s\n", FRANZ_VERSION);
      return 0;
    } else if (strcmp(argv[i], "-d") == 0) {
      debug = true;
      first_arg_index++;
    } else if (strcmp(argv[i], "--assert-types") == 0) {
      assert_types = true;
      first_arg_index++;
    } else if (strcmp(argv[i], "--no-tco") == 0) {
      // TCO: Disable tail call optimization (for debugging stack traces)
      enable_tco = false;
      first_arg_index++;
    } else if (strncmp(argv[i], "--scoping=", 10) == 0) {
      //  Parse --scoping=lexical or --scoping=dynamic
      const char* mode = argv[i] + 10;
      if (strcmp(mode, "lexical") == 0) {
        g_scoping_mode = SCOPING_LEXICAL;
      } else if (strcmp(mode, "dynamic") == 0) {
        g_scoping_mode = SCOPING_DYNAMIC;
      } else {
        fprintf(stderr, "Error: Invalid scoping mode '%s'. Use 'lexical' or 'dynamic'.\n", mode);
        return 1;
      }
      first_arg_index++;
    } else if (argv[i][0] == '-') {
      // unknown flag - ignore for now, keep compatibility
      first_arg_index++;
    } else {
      // first non-flag argument is the file path
      break;
    }
  }

  //  Debug output for scoping mode
  if (debug) {
    printf("Scoping mode: %s\n", ScopingMode_name(g_scoping_mode));
  }

  /* read file */
  if (debug) printf("\nCODE\n");
  char *code = NULL;
  long fileLength = 0;
  bool pipedInput = false;

  // Check if stdin is empty.
  fseek(stdin, 0, SEEK_END);
  bool stdinEmpty = ftell(stdin) == 0;
  rewind(stdin);


  if (argc > first_arg_index) {

    // if a path was supplied
    char *codePath = argv[first_arg_index];

    // if code is passed through an file argument
    FILE *p_file = fopen(codePath, "r");
    if (p_file == NULL) {
      printf("Error: Could not read file %s.\n", codePath);
      return 0;
    }

    // go to end, and record position (this will be the length of the file)
    fseek(p_file, 0, SEEK_END);
    fileLength = ftell(p_file);

    // rewind to start
    rewind(p_file);

    // allocate memory (+1 for 0 terminated string)
    code = malloc(fileLength + 1);

    // read file and close
    fread(code, fileLength, 1, p_file); 
    fclose(p_file);

    // set terminator to 0
    code[fileLength] = '\0';

  } else if (!stdinEmpty) {
    
    // if stdin is empty, read it (code was passed in as a pipe)
    code = malloc(0);

    // finalIndex tracks the last index populated with a non '\0' char
    int finalIndex = 0;
    char c = EOF;

    // loop through stdin.
    for (finalIndex = 0; (c = getchar()) != EOF; finalIndex += 1) {
      code = realloc(code, finalIndex + 1);
      code[finalIndex] = c;
    }

    // create space for null terminator
    code = realloc(code, finalIndex + 2);
    fileLength = finalIndex + 1;
    code[fileLength] = '\0';

    pipedInput = true;
  } else {
    printf("Error: Program not supplied through pipe or argument.\n");
    return 0;
  }
  
  if (debug) {
    printf("%s\n", code);
  }

  // Optional: type check before running
  if (assert_types) {
    if (debug) printf("Running type assertions (franz-check inline)\n");

    int ok = franz_assert_types(code, fileLength, debug ? 1 : 0);
    if (!ok) {
      free(code);
      return 1;
    }
  }

  // Calculate the number of arguments to skip (ie. name of executable, file passed, and flags).
  int argsToSkip = first_arg_index + (pipedInput ? 0 : 1);
  int exitCode = run(code, fileLength, argc - argsToSkip, &argv[argsToSkip], debug, enable_tco);

  // free code
  free(code);
  code = NULL;
  if (debug) printf("Code Freed\n");

  // Free module cache before exit
  ModuleCache_free();

  // Free circular dependency tracking
  CircularDeps_cleanup();

  // Free error handling system
  ErrorState_cleanup();

  return exitCode;
}
