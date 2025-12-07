#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>

#include "stdlib.h"
#include "generic.h"
#include "ast.h"
#include "scope.h"
#include "list.h"
#include "dict.h"
#include "eval_stub.h"  //  Temporary eval stub for runtime functions
#include "events.h"
#include "file.h"
#include "file-advanced/file_advanced.h"  //  Advanced file operations
#include "lex.h"
#include "closure/closure.h"  //  Closure support
#include "tokens.h"
#include "parse.h"
#include "module_cache.h"
#include "security/security.h"
#include "circular-deps/circular_deps.h"
#include "error-handling/error_handler.h"
#include "mutable-refs/ref.h"  //  Mutable reference support
#include "number-formats/number_parse.h"  // Multi-base & formatting

/* tools, used later in stdlib */
// validate number of arguments
void validateArgCount(int min, int max, int length, int lineNumber) {
  if (length > max) {
    // supplied too many args, throw error
    printf(
      "Runtime Error @ Line %i: Supplied more arguments than required to function.\n", 
      lineNumber
    );
    exit(0);
  } else if (length < min) {
    // supplied too little args, throw error
    printf(
      "Runtime Error @ Line %i: Supplied less arguments than required to function.\n", 
      lineNumber
    );
    exit(0);
  }
}

// verify that a bare minimum amount of args are passed
void validateMinArgCount(int min, int length, int lineNumber) {
  if (length < min) {
    // supplied too little args, throw error
    printf(
      "Runtime Error @ Line %i: Supplied less arguments than required to function.\n", 
      lineNumber
    );
    exit(0);
  }
}

// validate type of argument
void validateType(enum Type allowedTypes[], int typeCount, enum Type type, int argNum, int lineNumber, char* funcName) {
  bool valid = false;
  
  // check if type is valid
  for (int i = 0; i < typeCount; i++) {
    if (type == allowedTypes[i]) {
      valid = true;
      break;
    }
  }

  if (!valid) {
    // print error msg
    printf("Runtime Error @ Line %i: %s function requires ", lineNumber, funcName);

    for (int i = 0; i < typeCount - 1; i++) {
      printf("%s type or ", getTypeString(allowedTypes[i]));
    }

    printf("%s type", getTypeString(allowedTypes[typeCount - 1]));
    printf(" for argument #%i, %s type supplied instead.\n", argNum, getTypeString(type));

    exit(0);
  }
}

// validate that argument is binary
void validateBinary(int *p_val, int argNum, int lineNumber, char* funcName) {
  if (*(p_val) != 0 && *(p_val) != 1) {
    printf(
      "Runtime Error @ Line %i: %s function expected 0 or 1 for argument #%i, %i supplied instead.\n", 
      lineNumber, funcName, argNum, *p_val
    );
    exit(0);
  }
}

// validate that argument is within a range
void validateRange(int *p_val, int min, int max, int argNum, int lineNumber, char* funcName) {
  if (*p_val > max || *p_val < min) {
    printf(
      "Runtime Error @ Line %i: %s function expected a value in the range [%i, %i] for argument #%i, %i supplied instead.\n", 
      lineNumber, funcName, min, max, argNum, *p_val
    );

    exit(0);
  }
}

// validate that value is at least a minimum
void validateMin(int *p_val, int min, int argNum, int lineNumber, char* funcName) {
  if (*p_val < min) {
    printf(
      "Runtime Error @ Line %i: %s function expected a minimum value of %i for argument #%i, %i supplied instead.\n", 
      lineNumber, funcName, min, argNum, *p_val
    );

    exit(0);
  }
}

// applys a func, given arguments
// used for callbacks from the standard library
Generic *applyFunc(Generic *func, Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if(func->type == TYPE_NATIVEFUNCTION) {
    // native func case, simply obtain cb and run
    Generic *(*cb)(Scope *, Generic *[], int, int) = func->p_val;

    // increase ref count
    for (int i = 0; i < length; i++) {
      args[i]->refCount++;
    }

    Generic *res = cb(p_scope, args, length, lineNumber);

    // drop ref count, and free if count is 0
    for (int i = 0; i < length; i++) {
      args[i]->refCount--;
      if (args[i]->refCount == 0) Generic_free(args[i]);
    }

    if (func->refCount == 0) Generic_free(func);

    return res;

  } else if (func->type == TYPE_FUNCTION || func->type == TYPE_BYTECODE_CLOSURE || func->type == TYPE_BYTECODE_CLOSURE) {
    //  Handle lexical vs dynamic scoping
    // Extract the function AST and closure
    AstNode *p_fn_ast = NULL;
    Closure *p_closure_ptr = NULL;

    if (func->type == TYPE_FUNCTION) {
      // Old style: func->p_val is AstNode* directly 
      p_fn_ast = (AstNode *) func->p_val;
    } else {
      // New style: func->p_val is Closure*
      p_closure_ptr = (Closure *) func->p_val;
      p_fn_ast = p_closure_ptr->p_fn;
    }

    //  Choose parent scope based on scoping mode
    Scope *parent_scope;
    if (g_scoping_mode == SCOPING_LEXICAL && p_closure_ptr != NULL) {
      // LEXICAL: Use global scope as parent (builtins accessible)
      // Snapshot variables will be restored to local scope below
      Scope *global = p_scope;
      while (global->p_parent != NULL) {
        global = global->p_parent;
      }
      parent_scope = global;
    } else {
      // DYNAMIC: Use caller scope as parent (original behavior)
      parent_scope = p_scope;
    }

    // create local scope with chosen parent
    Scope *p_local = Scope_new(parent_scope);

    //  Restore snapshot ONLY in lexical mode
    // Dynamic mode uses caller scope directly, no snapshot needed
    if (g_scoping_mode == SCOPING_LEXICAL && p_closure_ptr != NULL && p_closure_ptr->p_snapshot != NULL) {
      // Iterate over snapshot and add each variable to local scope
      int keyCount = 0;
      Generic **keys = Dict_keys(p_closure_ptr->p_snapshot, &keyCount);

      for (int j = 0; j < keyCount; j++) {
        if (keys[j]->type == TYPE_STRING) {
          char *varName = *((char **) keys[j]->p_val);
          Generic *val = Dict_get(p_closure_ptr->p_snapshot, keys[j]);

          if (val != NULL) {
            // Scope_set will increment refCount itself
            Scope_set(p_local, varName, val, -1);  // Add to local scope
          }
        }
      }

      // Free keys array (Dict_keys allocates it)
      if (keys != NULL) free(keys);
    }

    //  Loop over function parameters (array-based)
    // Function children: [0..n-1]=parameters, [n]=body(statement)
    int funcParamCount = p_fn_ast->childCount - 1;  // Exclude body

    // Error handling
    if (length != funcParamCount) {
      if (length > funcParamCount) {
        printf("Runtime Error @ Line %i: Supplied more arguments than required to function.\n", lineNumber);
      } else {
        printf("Runtime Error @ Line %i: Supplied less arguments than required to function.\n", lineNumber);
      }
      Scope_free(p_local);
      exit(0);
    }

    // Populate local scope with arguments
    for (int i = 0; i < length; i++) {
      Scope_set(p_local, p_fn_ast->children[i]->val, args[i], -1);  // -1 = skip immutability check
    }

    // Last child is the statement body
    AstNode *p_curr = p_fn_ast->children[p_fn_ast->childCount - 1];

    // run statement with local scope
    Generic *res = eval(p_curr, p_local, 0);

    // free scope
    Scope_free(p_local);

    // free function if no references
    if (func->refCount == 0) Generic_free(func);
    
    return res;

  } else {
    printf(
      "Runtime Error @ Line %i: Attempted to call %s instead of function.\n", 
      lineNumber, getTypeString(func->type)
    );
    exit(0);
  }
}

/* IO */
// (print args...)
// prints given arguments
Generic *StdLib_print(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  for (int i = 0; i < length; i++) {
    Generic_print(args[i]);
    if (i < length - 1) printf(" ");
  }
  fflush(stdout);  // Flush output buffer to ensure immediate display

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (println args...)
// prints given arguments followed by a newline
Generic *StdLib_println(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  for (int i = 0; i < length; i++) {
    Generic_print(args[i]);
    if (i < length - 1) printf(" ");
  }
  printf("\n");
  return Generic_new(TYPE_VOID, NULL, 0);
}

// (input)
// gets input from stdin
Generic *StdLib_input(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(0, 0, length, lineNumber);

  // eat through anything in the input buffer
  enableRaw();
  char eat = readChar();
  while (eat != '\0') eat = readChar();
  disableRaw();

  // enable echo
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);

  // initial allocation of empty string
  char *res = (char *) malloc(sizeof(char));
  res[0] = '\0';

  // loop through every char
  char c = getchar();
  while (c != '\n') {
    // reallocate and add char
    res = realloc(res, sizeof(char) * (strlen(res) + 1 + 1));
    strncat(res, &c, 1);

    c = getchar();
  }

  // enable events again (turn off echo)
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &run_termios);

  // create pointer
  char **p_res = (char **) malloc(sizeof(char *));
  *p_res = res;

  return Generic_new(TYPE_STRING, p_res, 0);
}

// (columns)
// returns number of columns in terminal
Generic *StdLib_columns(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  // get current dimensions
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  // create pointer to rows
  int *rows = (int *) malloc(sizeof(int));
  *rows = w.ws_col;

  // return generic
  return Generic_new(TYPE_INT, rows, 0);
}

// (rows)
// returns number of rows in terminal
Generic *StdLib_rows(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  // get current dimensions
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  // create pointer to rows
  int *rows = (int *) malloc(sizeof(int));
  *rows = w.ws_row;

  // return generic
  return Generic_new(TYPE_INT, rows, 0);
}

// (read_file filepath)
// reads text at filepath and returns
Generic *StdLib_read_file(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  
  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "read_file");

  // read file
  char *res = readFile(*((char **) args[0]->p_val), false);
  
  // if file couldn't be read, return void
  if (res == NULL) return Generic_new(TYPE_VOID, NULL, 0);

  // malloc pointer to string
  char **p_res = (char **) malloc(sizeof(char *));
  *p_res = res;

  // return
  return Generic_new(TYPE_STRING, p_res, 0);
}

// (write_file filepath string)
// writes string to filepath
Generic *StdLib_write_file(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  // check that type of args is string
  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "write_file");
  validateType(allowedTypes, 1, args[1]->type, 2, lineNumber, "write_file");

  writeFile(*((char **) args[0]->p_val), *((char **) args[1]->p_val), lineNumber);
  return Generic_new(TYPE_VOID, NULL, 0);
}

//  (file_exists filepath)
// returns 1 if file exists and is readable, 0 otherwise
Generic *StdLib_file_exists(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "file_exists");

  // check if file exists
  int exists = fileExists(*((char **) args[0]->p_val));

  // malloc pointer to int
  int *p_res = (int *) malloc(sizeof(int));
  *p_res = exists;

  // return
  return Generic_new(TYPE_INT, p_res, 0);
}

//  (append_file filepath string)
// appends string to end of file
Generic *StdLib_append_file(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  // check that type of args is string
  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "append_file");
  validateType(allowedTypes, 1, args[1]->type, 2, lineNumber, "append_file");

  appendFile(*((char **) args[0]->p_val), *((char **) args[1]->p_val), lineNumber);
  return Generic_new(TYPE_VOID, NULL, 0);
}

// ============================================================================
//  Advanced File Operations
// ============================================================================

// (read_binary filepath)
// reads raw binary data from file, returns as string
Generic *StdLib_read_binary(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "read_binary");

  size_t size = 0;
  char *data = readBinary(*((char **) args[0]->p_val), &size);

  if (data == NULL) {
    // Return empty string on error
    char *empty = malloc(1);
    empty[0] = '\0';
    return Generic_new(TYPE_STRING, &empty, 0);
  }

  // Return binary data as string
  return Generic_new(TYPE_STRING, &data, 0);
}

// (write_binary filepath data)
// writes raw binary data to file
Generic *StdLib_write_binary(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "write_binary");
  validateType(allowedTypes, 1, args[1]->type, 2, lineNumber, "write_binary");

  char *path = *((char **) args[0]->p_val);
  char *data = *((char **) args[1]->p_val);
  size_t size = strlen(data);  // Get data length

  writeBinary(path, data, size, lineNumber);
  return Generic_new(TYPE_VOID, NULL, 0);
}

// (list_files dirpath)
// lists all files in directory, returns list of strings
Generic *StdLib_list_files(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "list_files");

  List *fileList = listFiles(*((char **) args[0]->p_val), lineNumber);

  return Generic_new(TYPE_LIST, fileList, 0);
}

// (create_dir dirpath)
// creates directory, returns 1 on success, 0 on failure
Generic *StdLib_create_dir(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "create_dir");

  int result = createDir(*((char **) args[0]->p_val), lineNumber);

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = result;
  return Generic_new(TYPE_INT, p_res, 0);
}

// (dir_exists dirpath)
// checks if directory exists, returns 1 if exists, 0 otherwise
Generic *StdLib_dir_exists(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dir_exists");

  int exists = dirExists(*((char **) args[0]->p_val));

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = exists;
  return Generic_new(TYPE_INT, p_res, 0);
}

// (remove_dir dirpath)
// removes empty directory, returns 1 on success, 0 on failure
Generic *StdLib_remove_dir(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "remove_dir");

  int result = removeDir(*((char **) args[0]->p_val), lineNumber);

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = result;
  return Generic_new(TYPE_INT, p_res, 0);
}

// (file_size filepath)
// gets file size in bytes, returns integer
Generic *StdLib_file_size(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "file_size");

  long size = fileSize(*((char **) args[0]->p_val));

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = (int)size;  // Cast long to int
  return Generic_new(TYPE_INT, p_res, 0);
}

// (file_mtime filepath)
// gets file modification time as Unix timestamp, returns integer
Generic *StdLib_file_mtime(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "file_mtime");

  long mtime = fileMtime(*((char **) args[0]->p_val));

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = (int)mtime;  // Cast long to int
  return Generic_new(TYPE_INT, p_res, 0);
}

// (is_directory path)
// checks if path is a directory, returns 1 if yes, 0 if no
Generic *StdLib_is_directory(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "is_directory");

  int isDir = isDirectory(*((char **) args[0]->p_val));

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = isDir;
  return Generic_new(TYPE_INT, p_res, 0);
}

// (event time) or (event)
// returns a string containing the current event, blocking up to time seconds
Generic *StdLib_event(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(0, 1, length, lineNumber);

  // find the number of deciseconds to block
  int decisecondsBlock;
  if (length == 1) {
    enum Type allowedTypes[] = {TYPE_FLOAT, TYPE_INT};
    validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "event");
  
    if (args[0]->type == TYPE_INT) {
      decisecondsBlock = *((int *) args[0]->p_val) * 10;
    }
    if (args[0]->type == TYPE_FLOAT) {
      decisecondsBlock = (int) ceilf(*((double *) args[0]->p_val) * 10);
    }
  }

  char **p_res = (char **) malloc(sizeof(char *));

  int i = 0;
  while (length == 0 || i < decisecondsBlock) {
    if (i != 0) {
      free(*p_res);
    }
    *p_res = event();

    // if an event is received
    if (strcmp(*p_res, "") != 0) {
      return Generic_new(TYPE_STRING, p_res, 0);
    }

    i += 1;
  }
  
  return Generic_new(TYPE_STRING, p_res, 0);
}

// (use path1 path2 path3 ... fn)
// creates a new scope, evaluates the code in path1, path2, and path3, and then uses said scope to evaluate fn
Generic *StdLib_use(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // validate that all arguments with exception of last one are strings
  for (int i = 0; i < length - 1; i++) {
    enum Type allowedTypes[] = {TYPE_STRING};
    validateType(allowedTypes, 1, args[i]->type, i + 1, lineNumber, "use");
  }

  // validate that last argument is a function
  enum Type allowedTypes[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes, 3, args[length - 1]->type, length, lineNumber, "use");

  // create a new scope for all paths and fn to run in
  Scope *p_newScope = Scope_new(p_scope);

  // for each path
  for (int i = 0; i < length - 1; i++) {
    char *module_path = *((char **) args[i]->p_val);
    AstNode *p_headAstNode = NULL;

    // Check for circular dependency BEFORE loading
    if (CircularDeps_push(module_path, lineNumber) != 0) {
      // Circular dependency detected!
      CircularDeps_printChain(module_path);
      Scope_free(p_newScope);
      exit(1);
    }

    // Check module cache first
    CachedModule *cached = ModuleCache_read(module_path);
    if (cached != NULL && cached->p_ast != NULL) {
      // Use cached AST (make a copy to eval)
      p_headAstNode = AstNode_copy(cached->p_ast, 1);
    } else {
      // Not cached - read, lex, parse, and cache

      // read file
      char *code = readFile(module_path, true);

      // throw error if file could not be read
      if (code == NULL) {
        printf(
          "Runtime Error @ Line %i: Cannot read file \"%s\".\n",
          lineNumber, module_path
        );
        exit(0);
      }

      //  Array-based lexing
      TokenArray *tokens = lex(code, strlen(code));

      //  Array-based parsing
      p_headAstNode = parseProgram(tokens);

      // Cache the parsed AST for future use
      struct stat st;
      long mtime = 0;
      if (stat(module_path, &st) == 0) {
        mtime = st.st_mtime;
      }
      ModuleCache_write(module_path, p_headAstNode, mtime);

      // free memory
      free(code);
      TokenArray_free(tokens);
    }

    // eval and free AST
    Generic_free(eval(p_headAstNode, p_newScope, 0));
    AstNode_free(p_headAstNode);

    // Pop from import stack after successful load
    CircularDeps_pop(module_path);
  }

  // apply callback with new scope
  Generic *res = applyFunc(args[length - 1], p_newScope, NULL, 0, lineNumber);

  // free new scope and return
  Scope_free(p_newScope);
  return res;
}

// (use_as path)
// creates a namespace from a module file
// Returns a namespace object containing all symbols from the module
Generic *StdLib_use_as(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  // validate that argument is a string (file path)
  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "use_as");

  char *module_path = *((char **) args[0]->p_val);
  AstNode *p_headAstNode = NULL;

  // Check for circular dependency BEFORE loading
  if (CircularDeps_push(module_path, lineNumber) != 0) {
    // Circular dependency detected!
    CircularDeps_printChain(module_path);
    exit(1);
  }

  // Check module cache first
  CachedModule *cached = ModuleCache_read(module_path);
  if (cached != NULL && cached->p_ast != NULL) {
    // Use cached AST (make a copy to eval)
    p_headAstNode = AstNode_copy(cached->p_ast, 1);
  } else {
    // Not cached - read, lex, parse, and cache

    // read file
    char *code = readFile(module_path, true);

    // throw error if file could not be read
    if (code == NULL) {
      printf(
        "Runtime Error @ Line %i: Cannot read file \"%s\".\n",
        lineNumber, module_path
      );
      exit(0);
    }

    //  Array-based lexing
    TokenArray *tokens = lex(code, strlen(code));

    //  Array-based parsing
    p_headAstNode = parseProgram(tokens);

    // Cache the parsed AST for future use
    struct stat st;
    long mtime = 0;
    if (stat(module_path, &st) == 0) {
      mtime = st.st_mtime;
    }
    ModuleCache_write(module_path, p_headAstNode, mtime);

    // free memory
    free(code);
    TokenArray_free(tokens);
  }

  // Create a new isolated scope for the namespace (inherits from global for stdlib)
  Scope *p_namespaceScope = Scope_new(p_scope->p_parent);

  // eval module code in the isolated namespace scope
  Generic_free(eval(p_headAstNode, p_namespaceScope, 0));
  AstNode_free(p_headAstNode);

  // Pop from import stack after successful load
  CircularDeps_pop(module_path);

  // Return namespace object (Scope* wrapped in TYPE_NAMESPACE Generic)
  return Generic_new(TYPE_NAMESPACE, p_namespaceScope, 0);
}

// (use_with capabilities path1 path2 ... fn)
// Capability-based sandboxed imports - imported code only has access to granted capabilities
// First arg must be a list of capability strings, last arg must be a function
// Example: (use_with (list "print" "arithmetic") "lib.franz" { -> (main) })
Generic *StdLib_use_with(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(3, length, lineNumber);

  // Validate first argument is a list (capabilities)
  enum Type capListType[] = {TYPE_LIST};
  validateType(capListType, 1, args[0]->type, 1, lineNumber, "use_with");

  // Validate all middle arguments are strings (file paths)
  for (int i = 1; i < length - 1; i++) {
    enum Type allowedTypes[] = {TYPE_STRING};
    validateType(allowedTypes, 1, args[i]->type, i + 1, lineNumber, "use_with");
  }

  // Validate last argument is a function
  enum Type allowedTypes[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes, 3, args[length - 1]->type, length, lineNumber, "use_with");

  // Create an isolated scope with NO parent (complete sandboxing)
  Scope *p_capScope = Security_createIsolatedScope();

  // Seed the capability scope with granted capabilities
  if (Security_seedCapabilities(p_capScope, args[0], lineNumber) != 0) {
    Scope_free(p_capScope);
    exit(0);
  }

  // Load and evaluate each module file in the capability-restricted scope
  for (int i = 1; i < length - 1; i++) {
    char *module_path = *((char **) args[i]->p_val);
    AstNode *p_headAstNode = NULL;

    // Check for circular dependency BEFORE loading
    if (CircularDeps_push(module_path, lineNumber) != 0) {
      // Circular dependency detected!
      CircularDeps_printChain(module_path);
      Scope_free(p_capScope);
      exit(1);
    }

    // Check module cache first
    CachedModule *cached = ModuleCache_read(module_path);
    if (cached != NULL && cached->p_ast != NULL) {
      // Use cached AST (make a copy to eval)
      p_headAstNode = AstNode_copy(cached->p_ast, 1);
    } else {
      // Not cached - read, lex, parse, and cache

      // read file
      char *code = readFile(module_path, true);

      // throw error if file could not be read
      if (code == NULL) {
        printf(
          "Runtime Error @ Line %i: Cannot read file \"%s\".\n",
          lineNumber, module_path
        );
        Scope_free(p_capScope);
        exit(0);
      }

      //  Array-based lexing
      TokenArray *tokens = lex(code, strlen(code));

      //  Array-based parsing
      p_headAstNode = parseProgram(tokens);

      // Cache the parsed AST for future use
      struct stat st;
      long mtime = 0;
      if (stat(module_path, &st) == 0) {
        mtime = st.st_mtime;
      }
      ModuleCache_write(module_path, p_headAstNode, mtime);

      // free memory
      free(code);
      TokenArray_free(tokens);
    }

    // eval module in the capability-restricted scope
    Generic_free(eval(p_headAstNode, p_capScope, 0));
    AstNode_free(p_headAstNode);

    // Pop from import stack after successful load
    CircularDeps_pop(module_path);
  }

  // Apply callback with capability-restricted scope
  Generic *res = applyFunc(args[length - 1], p_capScope, NULL, 0, lineNumber);

  // free capability scope and return
  Scope_free(p_capScope);
  return res;
}

// (shell command)
// runs command with popen, returns stdout
Generic *StdLib_shell(Scope *p_scope, Generic *args[], int length, int lineNumber) {

  // validation
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "shell");

  // open process to run file
  FILE *p_out = popen(*((char **) args[0]->p_val), "r");

  // ensure process returned
  if (p_out == NULL) {
    printf(
      "Runtime Error @ Line %i: Failed to run shell command.\n", 
      lineNumber
    );
    exit(0);
  }

  char *res = malloc(sizeof(char));
  res[0] = '\0';

  // get stdout
  char c;
  while (true) {
    // add char to res
    res = realloc(res, sizeof(char) * (strlen(res) + 1 + 1));
    c = fgetc(p_out);
    if (feof(p_out)) break; // end of stdout
    strncat(res, &c, 1);
  }

  // close process
  pclose(p_out);

  // create pointer for generic to return
  char **p_res = malloc(sizeof(char *));
  *p_res = res;
  
  return Generic_new(TYPE_STRING, p_res, 0);
}

/* comparissions */
// (is a b)
// checks for equality between a and b
// returns 1 for true, 0 for false
Generic *StdLib_is(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  // return
  int *res = (int *) malloc(sizeof(int));
  *res = Generic_is(args[0], args[1]);
  return Generic_new(TYPE_INT, res, 0);
}

// (less_than a b)
// checks if a is less than b
Generic *StdLib_less_than(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);
  
  enum Type allowedTypes[] = {TYPE_FLOAT, TYPE_INT};

  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "less_than");
  validateType(allowedTypes, 3, args[1]->type, 2, lineNumber, "less_than");

  // do comparision and return
  Generic *a = args[0];
  Generic *b = args[1];
  int *p_res = (int *) malloc(sizeof(int));
  *p_res = (
    (a->type == TYPE_FLOAT ? *((double *) a->p_val) : *((int *) a->p_val))
    < (b->type == TYPE_FLOAT ? *((double *) b->p_val) : *((int *) b->p_val))
  );

  return Generic_new(TYPE_INT, p_res, 0); 
}

// (greater_than a b)
// checks if a is greater than b
Generic *StdLib_greater_than(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);
  
  enum Type allowedTypes[] = {TYPE_FLOAT, TYPE_INT};

  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "greater_than");
  validateType(allowedTypes, 3, args[1]->type, 2, lineNumber, "greater_than");

  // do comparision and return
  Generic *a = args[0];
  Generic *b = args[1];
  int *p_res = (int *) malloc(sizeof(int));
  *p_res = (
    (a->type == TYPE_FLOAT ? *((double *) a->p_val) : *((int *) a->p_val))
    > (b->type == TYPE_FLOAT ? *((double *) b->p_val) : *((int *) b->p_val))
  );

  return Generic_new(TYPE_INT, p_res, 0); 
}

/* logical operators */
// (not a)
// returns 1 if a = 0, 0 if a = 1
Generic *StdLib_not(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "not");

  validateBinary(args[0]->p_val, 1, lineNumber, "not");

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = 1 - *((int *) args[0]->p_val);

  return Generic_new(TYPE_INT, p_res, 0);
}

// (and a b)
// returns a and b
Generic *StdLib_and(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // type check
  enum Type allowedTypes[] = {TYPE_INT};
  for (int i = 0; i < length; i++) {
    validateType(allowedTypes, 1, args[i]->type, i + 1, lineNumber, "and");
    validateBinary(args[i]->p_val, i + 1, lineNumber, "and");
  };

  // set up result
  int *p_res = (int *) malloc(sizeof(int));
  *p_res = 1;

  // add each arg to *p_res
  for (int i = 0; i < length; i++) {
    *p_res = *p_res && *((int *) args[i]->p_val);
  }

  return Generic_new(TYPE_INT, p_res, 0);
}

// (or a b ...)
// returns a or b
Generic *StdLib_or(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // type check
  enum Type allowedTypes[] = {TYPE_INT};
  for (int i = 0; i < length; i++) {
    validateType(allowedTypes, 1, args[i]->type, i + 1, lineNumber, "or");
    validateBinary(args[i]->p_val, i + 1, lineNumber, "or");
  };

  // set up result
  int *p_res = (int *) malloc(sizeof(int));
  *p_res = 0;

  // add each arg to *p_res
  for (int i = 0; i < length; i++) {
    *p_res = *p_res || *((int *) args[i]->p_val);
  }

  return Generic_new(TYPE_INT, p_res, 0);
}

/* arithmetic */
// (add arg1 arg2 arg3 ...)
// sums all args
Generic *StdLib_add(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // flag if we can return integer, or if we must return float
  bool resIsInt = true;

  // type check
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  for (int i = 0; i < length; i++) {
    resIsInt = args[i]->type == TYPE_INT && resIsInt;
    validateType(allowedTypes, 3, args[i]->type, i + 1, lineNumber, "add");
  };
  
  if (resIsInt) {
    // case where we can return int
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = 0;

    // add each arg to *p_res
    for (int i = 0; i < length; i++) {
      *p_res += *((int *) args[i]->p_val);
    }

    return Generic_new(TYPE_INT, p_res, 0);

  } else {
    // case where we must return float
    double *p_res = (double *) malloc(sizeof(double));
    *p_res = 0;

    // add each arg to *p_res
    for (int i = 0; i < length; i++) {
      *p_res += args[i]->type == TYPE_FLOAT 
        ? *((double *) args[i]->p_val) 
        : *((int *) args[i]->p_val);
    }

    return Generic_new(TYPE_FLOAT, p_res, 0);
  }
}

// (subtract arg1 arg2 arg3 ...)
// returns arg1 - arg2 - arg3 - ...
Generic *StdLib_subtract(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // flag if we can return integer, or if we must return float
  bool resIsInt = true;

  // type check
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  for (int i = 0; i < length; i++) {
    resIsInt = args[i]->type == TYPE_INT && resIsInt;
    validateType(allowedTypes, 3, args[i]->type, i + 1, lineNumber, "subtract");
  };
  
  if (resIsInt) {
    // case where we can return int
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = *((int *) args[0]->p_val);

    // subtract each arg from *p_res
    for (int i = 1; i < length; i++) {
      *p_res -= *((int *) args[i]->p_val);
    }

    return Generic_new(TYPE_INT, p_res, 0);

  } else {
    // case where we must return float
    double *p_res = (double *) malloc(sizeof(double));
    *p_res = args[0]->type == TYPE_FLOAT 
        ? *((double *) args[0]->p_val) 
        : *((int *) args[0]->p_val);;

    // subtract each arg from *p_res
    for (int i = 1; i < length; i++) {
      *p_res -= args[i]->type == TYPE_FLOAT 
        ? *((double *) args[i]->p_val) 
        : *((int *) args[i]->p_val);
    }

    return Generic_new(TYPE_FLOAT, p_res, 0);
  }
}

// (divide arg1 arg2 arg3 ...)
// returns arg1 / arg2 / arg3 / ...
Generic *StdLib_divide(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // type check
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  for (int i = 0; i < length; i++) {
    validateType(allowedTypes, 3, args[i]->type, i + 1, lineNumber, "divide");
  };
  
  // initial value
  double *p_res = (double *) malloc(sizeof(double));
  *p_res = args[0]->type == TYPE_FLOAT 
      ? *((double *) args[0]->p_val) 
      : *((int *) args[0]->p_val);;

  // divide each arg from *p_res
  for (int i = 1; i < length; i++) {
    double val = args[i]->type == TYPE_FLOAT 
      ? *((double *) args[i]->p_val) 
      : *((int *) args[i]->p_val);

    if (val == 0) {
      // throw error for division by 0
      printf(
        "Runtime Error @ Line %i: Division by 0.\n", 
        lineNumber
      );
      exit(0);
    };

    *p_res /= val;
  }

  return Generic_new(TYPE_FLOAT, p_res, 0);
}

// (multiply arg1 arg2 arg3 ...)
// returns arg1 * arg2 * arg3 * ...
Generic *StdLib_multiply(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // flag if we can return integer, or if we must return float
  bool resIsInt = true;

  // type check
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  for (int i = 0; i < length; i++) {
    resIsInt = args[i]->type == TYPE_INT && resIsInt;
    validateType(allowedTypes, 3, args[i]->type, i + 1, lineNumber, "multiply");
  };
  
  if (resIsInt) {
    // case where we can return int
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = *((int *) args[0]->p_val);;

    // multiply each arg to *p_res
    for (int i = 1; i < length; i++) {
      *p_res *= *((int *) args[i]->p_val);
    }

    return Generic_new(TYPE_INT, p_res, 0);

  } else {
    // case where we must return float
    double *p_res = (double *) malloc(sizeof(double));
    *p_res = args[0]->type == TYPE_FLOAT 
        ? *((double *) args[0]->p_val) 
        : *((int *) args[0]->p_val);

    // multiply each arg to *p_res
    for (int i = 1; i < length; i++) {
      *p_res *= args[i]->type == TYPE_FLOAT 
        ? *((double *) args[i]->p_val) 
        : *((int *) args[i]->p_val);
    }

    return Generic_new(TYPE_FLOAT, p_res, 0);
  }
}

// (remainder a b)
// returns the a remainder b
Generic *StdLib_remainder(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);
  
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "remainder");
  validateType(allowedTypes, 3, args[1]->type, 2, lineNumber, "remainder");

  if ((args[1]->type == TYPE_FLOAT ? *((double *) args[1]->p_val) : *((int *) args[1]->p_val)) == 0) {
    // throw error for division by 0
    printf(
      "Runtime Error @ Line %i: Remainder of division by 0.\n", 
      lineNumber
    );
    exit(0);
  };

  if (args[0]->type == TYPE_INT && args[1]->type == TYPE_INT) {
    // if we can return integer
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = *((int *) args[0]->p_val) % *((int *) args[1]->p_val);
    return Generic_new(TYPE_INT, p_res, 0);
  } else {
    // if we must return float
    double *p_res = (double *) malloc(sizeof(double));

    *p_res = fmod(
      (args[0]->type == TYPE_FLOAT ? *((double *) args[0]->p_val) : *((int *) args[0]->p_val)),
      (args[1]->type == TYPE_FLOAT ? *((double *) args[1]->p_val) : *((int *) args[1]->p_val))
    );

    return Generic_new(TYPE_FLOAT, p_res, 0);
  }
}

// (power a b)
// returns the a to the power of b
Generic *StdLib_power(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);
  
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "power");
  validateType(allowedTypes, 3, args[1]->type, 2, lineNumber, "power");

  if (args[0]->type == TYPE_INT && args[1]->type == TYPE_INT) {
    // if we can return integer
    int *p_res = (int *) malloc(sizeof(int));

    *p_res = pow(
      *((int *) args[0]->p_val),
      *((int *) args[1]->p_val)
    );

    return Generic_new(TYPE_INT, p_res, 0);
  } else {
    // if we must return float
    double *p_res = (double *) malloc(sizeof(double));

    *p_res = pow(
      (args[0]->type == TYPE_FLOAT ? *((double *) args[0]->p_val) : *((int *) args[0]->p_val)),
      (args[1]->type == TYPE_FLOAT ? *((double *) args[1]->p_val) : *((int *) args[1]->p_val))
    );

    return Generic_new(TYPE_FLOAT, p_res, 0);
  }
}

// (random)
// returns random number from 0 to 1
Generic *StdLib_random(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(0, 0, length, lineNumber);

  double *p_res = (double *) malloc(sizeof(double));
  *p_res = (double) rand() / (double) RAND_MAX;
  
  return Generic_new(TYPE_FLOAT, p_res, 0);
}

// (floor n)
// returns the largest integer less than or equal to n
Generic *StdLib_floor(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "floor");

  if (args[0]->type == TYPE_INT) {
    // Already an integer, return as-is
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = *((int *) args[0]->p_val);
    return Generic_new(TYPE_INT, p_res, 0);
  } else {
    // Float - use floor function
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = (int) floor(*((double *) args[0]->p_val));
    return Generic_new(TYPE_INT, p_res, 0);
  }
}

// (ceil n)
// returns the smallest integer greater than or equal to n
Generic *StdLib_ceil(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "ceil");

  if (args[0]->type == TYPE_INT) {
    // Already an integer, return as-is
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = *((int *) args[0]->p_val);
    return Generic_new(TYPE_INT, p_res, 0);
  } else {
    // Float - use ceil function
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = (int) ceil(*((double *) args[0]->p_val));
    return Generic_new(TYPE_INT, p_res, 0);
  }
}

// (round n)
// returns the nearest integer to n (rounds half-up)
Generic *StdLib_round(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "round");

  if (args[0]->type == TYPE_INT) {
    // Already an integer, return as-is
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = *((int *) args[0]->p_val);
    return Generic_new(TYPE_INT, p_res, 0);
  } else {
    // Float - use round function
    int *p_res = (int *) malloc(sizeof(int));
    *p_res = (int) round(*((double *) args[0]->p_val));
    return Generic_new(TYPE_INT, p_res, 0);
  }
}

// (abs n)
// returns the absolute value of n
Generic *StdLib_abs(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "abs");

  if (args[0]->type == TYPE_INT) {
    int *p_res = (int *) malloc(sizeof(int));
    int val = *((int *) args[0]->p_val);
    *p_res = val < 0 ? -val : val;
    return Generic_new(TYPE_INT, p_res, 0);
  } else {
    double *p_res = (double *) malloc(sizeof(double));
    *p_res = fabs(*((double *) args[0]->p_val));
    return Generic_new(TYPE_FLOAT, p_res, 0);
  }
}

// (min ...)
// returns the minimum value from the arguments
Generic *StdLib_min(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};

  // Validate all arguments are numbers
  for (int i = 0; i < length; i++) {
    validateType(allowedTypes, 2, args[i]->type, i + 1, lineNumber, "min");
  }

  // Determine if result should be int or float
  int hasFloat = 0;
  for (int i = 0; i < length; i++) {
    if (args[i]->type == TYPE_FLOAT) {
      hasFloat = 1;
      break;
    }
  }

  if (hasFloat) {
    // Find minimum as float
    double minVal = args[0]->type == TYPE_FLOAT ?
      *((double *) args[0]->p_val) : (double)(*((int *) args[0]->p_val));

    for (int i = 1; i < length; i++) {
      double val = args[i]->type == TYPE_FLOAT ?
        *((double *) args[i]->p_val) : (double)(*((int *) args[i]->p_val));
      if (val < minVal) minVal = val;
    }

    double *p_res = (double *) malloc(sizeof(double));
    *p_res = minVal;
    return Generic_new(TYPE_FLOAT, p_res, 0);
  } else {
    // All integers
    int minVal = *((int *) args[0]->p_val);
    for (int i = 1; i < length; i++) {
      int val = *((int *) args[i]->p_val);
      if (val < minVal) minVal = val;
    }

    int *p_res = (int *) malloc(sizeof(int));
    *p_res = minVal;
    return Generic_new(TYPE_INT, p_res, 0);
  }
}

// (max ...)
// returns the maximum value from the arguments
Generic *StdLib_max(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};

  // Validate all arguments are numbers
  for (int i = 0; i < length; i++) {
    validateType(allowedTypes, 2, args[i]->type, i + 1, lineNumber, "max");
  }

  // Determine if result should be int or float
  int hasFloat = 0;
  for (int i = 0; i < length; i++) {
    if (args[i]->type == TYPE_FLOAT) {
      hasFloat = 1;
      break;
    }
  }

  if (hasFloat) {
    // Find maximum as float
    double maxVal = args[0]->type == TYPE_FLOAT ?
      *((double *) args[0]->p_val) : (double)(*((int *) args[0]->p_val));

    for (int i = 1; i < length; i++) {
      double val = args[i]->type == TYPE_FLOAT ?
        *((double *) args[i]->p_val) : (double)(*((int *) args[i]->p_val));
      if (val > maxVal) maxVal = val;
    }

    double *p_res = (double *) malloc(sizeof(double));
    *p_res = maxVal;
    return Generic_new(TYPE_FLOAT, p_res, 0);
  } else {
    // All integers
    int maxVal = *((int *) args[0]->p_val);
    for (int i = 1; i < length; i++) {
      int val = *((int *) args[i]->p_val);
      if (val > maxVal) maxVal = val;
    }

    int *p_res = (int *) malloc(sizeof(int));
    *p_res = maxVal;
    return Generic_new(TYPE_INT, p_res, 0);
  }
}

// (sqrt n)
// returns the square root of n
Generic *StdLib_sqrt(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "sqrt");

  double *p_res = (double *) malloc(sizeof(double));

  if (args[0]->type == TYPE_INT) {
    *p_res = sqrt((double)(*((int *) args[0]->p_val)));
  } else {
    *p_res = sqrt(*((double *) args[0]->p_val));
  }

  return Generic_new(TYPE_FLOAT, p_res, 0);
}

// (random_int n)
// returns a random integer in range [0, n) - excludes n
Generic *StdLib_random_int(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "random_int");

  int maxVal = *((int *) args[0]->p_val);

  if (maxVal <= 0) {
    printf("Runtime Error @ Line %i: random_int argument must be positive (got %d).\n",
           lineNumber, maxVal);
    exit(0);
  }

  int *p_res = (int *) malloc(sizeof(int));
  *p_res = rand() % maxVal;

  return Generic_new(TYPE_INT, p_res, 0);
}

// (random_range min max)
// returns a random float in range [min, max) - includes min, excludes max
Generic *StdLib_random_range(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 2, args[0]->type, 1, lineNumber, "random_range");
  validateType(allowedTypes, 2, args[1]->type, 2, lineNumber, "random_range");

  // Convert both args to doubles
  double minVal = args[0]->type == TYPE_FLOAT ?
    *((double *) args[0]->p_val) : (double)(*((int *) args[0]->p_val));
  double maxVal = args[1]->type == TYPE_FLOAT ?
    *((double *) args[1]->p_val) : (double)(*((int *) args[1]->p_val));

  if (minVal >= maxVal) {
    printf("Runtime Error @ Line %i: random_range min must be less than max (got min=%f, max=%f).\n",
           lineNumber, minVal, maxVal);
    exit(0);
  }

  double *p_res = (double *) malloc(sizeof(double));
  double range = maxVal - minVal;
  *p_res = minVal + (((double) rand() / (double) RAND_MAX) * range);

  return Generic_new(TYPE_FLOAT, p_res, 0);
}

// (random_seed n)
// seeds the random number generator with n for reproducible sequences
// returns void
Generic *StdLib_random_seed(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "random_seed");

  int seed = *((int *) args[0]->p_val);
  srand((unsigned int)seed);

  return Generic_new(TYPE_VOID, NULL, 0);
}

/* control */
// (loop n f)
// applys f, n times, passing the current index to f
// returns whatever f returns if not void
// will go to completion and return void if void is all f returns
Generic *StdLib_loop(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_INT};
  enum Type allowedTypes2[] = {TYPE_NATIVEFUNCTION, TYPE_FUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures

  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "loop");
  validateType(allowedTypes2, 3, args[1]->type, 2, lineNumber, "loop");  //  3 types now

  validateMin(args[0]->p_val, 0, 1, lineNumber, "loop");
  
  // loop
  for (int i = 0; i < *((int *) args[0]->p_val); i++) {

    // get arg to pass to cb
    int *p_arg = (int *) malloc(sizeof(int));
    *p_arg = i;
    Generic *newArgs[1] = {Generic_new(TYPE_INT, p_arg, 0)};
    
    // call cb
    Generic *res = applyFunc(args[1], p_scope, newArgs, 1, lineNumber);

    // if void type returned, free
    if (res->type != TYPE_VOID) return res;
    else Generic_free(res);
  }

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (until stop f initial)
// runs until f returns stop
// passes the last returned value to f, as well as the current index
// intial acts like the first "state", unless another state supplied
Generic *StdLib_until(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 3, length, lineNumber);

  // verify 2nd arg is a function ( include TYPE_BYTECODE_CLOSURE)
  enum Type allowedTypes[] = {TYPE_NATIVEFUNCTION, TYPE_FUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};
  validateType(allowedTypes, 3, args[1]->type, 2, lineNumber, "until");

  // set up state
  Generic *state;

  if (length == 3) {
    state = Generic_copy(args[2]);
  } else {
    state = Generic_new(TYPE_VOID, NULL, 0);
  }

  // flags and index
  int i = 0;

  // loop
  while (true) {

    // get index
    int *p_i = (int *) malloc(sizeof(int));
    *p_i = i;
    Generic *newArgs[2] = {Generic_copy(state), Generic_new(TYPE_INT, p_i, 0)};

    // callback
    Generic *res = applyFunc(args[1], p_scope, newArgs, 2, lineNumber);

    // handle state
    int comp = Generic_is(res, args[0]);
    if (comp) {
      Generic_free(res);
      return state;
    } else {
      Generic_free(state);
      state = res;
    }

    i++;
  }
}

// (if c1 f1 c2 f2 c3 f3 ... else)
// applys f1 if c1 == 1, etc.
// otherwise run else
Generic *StdLib_if(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  enum Type allowedTypesCond[] = {TYPE_INT};
  enum Type allowedTypesCb[] = {TYPE_NATIVEFUNCTION, TYPE_FUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures

  bool conditionPassed = false;

  for (int i = 0; i < length; i++) {
    if (i % 2 == 0) {
      if (i == length - 1) {
        // else case
        validateType(allowedTypesCb, 3, args[i]->type, i + 1, lineNumber, "if");  //  3 types now
        return applyFunc(args[i], p_scope, NULL, 0, lineNumber);

      } else {
        // condition case
        validateType(allowedTypesCond, 1, args[i]->type, i + 1, lineNumber, "if");
        validateBinary(args[i]->p_val, i + 1, lineNumber, "if");

        // find if condition is true
        conditionPassed = *((int *) args[i]->p_val) == 1;
      }
    } else {
      // callback case
      validateType(allowedTypesCb, 3, args[i]->type, i + 1, lineNumber, "if");  //  3 types now

      // case where callback can be evaluated
      if (conditionPassed) {
        return applyFunc(args[i], p_scope, NULL, 0, lineNumber);
      }
    }
  }

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (wait t)
// waits t seconds
Generic *StdLib_wait(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  enum Type allowedTypes[] = {TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "wait");
  int initialTime = clock();
  while (
    (((double) clock()) - ((double) initialTime)) / ((double) CLOCKS_PER_SEC)
    < (args[0]->type == TYPE_INT ? *((int *) args[0]->p_val) : *((double *) args[0]->p_val))
  ) {}

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (time)
// Returns current time in seconds since program start (float)
Generic *StdLib_time(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(0, 0, length, lineNumber);

  double time_seconds = ((double) clock()) / ((double) CLOCKS_PER_SEC);
  double *p_time = malloc(sizeof(double));
  *p_time = time_seconds;

  return Generic_new(TYPE_FLOAT, p_time, lineNumber);
}

/* error handling */
// (try {try-block} {error -> handler-block})
// Execute try-block, if error occurs, run handler-block with error info
// Returns result of try-block on success, result of handler on error
Generic *StdLib_try(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "try");
  validateType(allowedTypes, 3, args[1]->type, 2, lineNumber, "try");

  // Enter try block (increment depth)
  ErrorState_enterTry();

  // Execute try block
  Generic *result = applyFunc(args[0], p_scope, NULL, 0, lineNumber);

  // Check if an error occurred
  if (ErrorState_hasError()) {
    // Error occurred - call error handler

    // Get error message
    const char *error_msg = ErrorState_getMessage();

    // Create error message string to pass to handler
    char **p_msg = malloc(sizeof(char *));
    *p_msg = strdup(error_msg);
    Generic *error_obj = Generic_new(TYPE_STRING, p_msg, 0);

    // Clear the error before calling handler
    ErrorState_clearError();

    // Exit try block
    ErrorState_exitTry();

    // Call error handler with error message
    Generic *handler_args[] = {error_obj};
    Generic *handler_result = applyFunc(args[1], p_scope, handler_args, 1, lineNumber);

    // Don't free error_obj - let garbage collector handle it
    // Generic_free(error_obj);

    // Don't free try block result - let garbage collector handle it

    return handler_result;
  } else {
    // Success - exit try block and return result
    ErrorState_exitTry();
    return result;
  }
}

// (catch {try-block} {fallback-value})
// Simplified error handler - returns fallback-value on any error
// More convenient than try when you don't need error details
Generic *StdLib_catch(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "catch");

  // Enter try block
  ErrorState_enterTry();

  // Execute try block
  Generic *result = applyFunc(args[0], p_scope, NULL, 0, lineNumber);

  // Check if an error occurred
  if (ErrorState_hasError()) {
    // Error occurred - clear it and return fallback
    ErrorState_clearError();
    ErrorState_exitTry();

    // Don't free try block result - let garbage collector handle it

    //  Return fallback - evaluate if it's a function (including closures)
    if (args[1]->type == TYPE_FUNCTION || args[1]->type == TYPE_NATIVEFUNCTION || args[1]->type == TYPE_BYTECODE_CLOSURE || args[1]->type == TYPE_BYTECODE_CLOSURE) {
      return applyFunc(args[1], p_scope, NULL, 0, lineNumber);
    } else {
      return Generic_copy(args[1]);
    }
  } else {
    // Success - exit try block and return result
    ErrorState_exitTry();
    return result;
  }
}

// (error message)
// Throw a custom error with message
// Can be caught by try/catch blocks
Generic *StdLib_error(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "error");

  char *message = *((char **) args[0]->p_val);

  // Set error state (will exit if not in try block)
  ErrorState_setError(ERROR_CUSTOM, lineNumber, "%s", message);

  // If we reach here, we're in a try block - return void
  // (the try block will check for error and handle it)
  return Generic_new(TYPE_VOID, NULL, 0);
}

/* types */
// (int x)
// returns x as an integer, if string, float, or int passed
Generic *StdLib_integer(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING, TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "integer");

  int *p_res = malloc(sizeof(int));

  if (args[0]->type == TYPE_STRING) {
    char *str = *((char **) args[0]->p_val);
    *p_res = atoi(str);
  } else if (args[0]->type == TYPE_FLOAT) {
    double f = *((double *) args[0]->p_val);
    *p_res = (int) f;
  } else {
    int i = *((int *) args[0]->p_val);
    *p_res = i;
  }

  return Generic_new(TYPE_INT, p_res, 0);
}

// (string x)
// returns x as a string, if string, float, or int passed
Generic *StdLib_string(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING, TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "string");
  
  char *res;

  if (args[0]->type == TYPE_FLOAT) {
    int length = snprintf(NULL, 0, "%f", *((double *) args[0]->p_val)); // get length
    res = malloc(sizeof(char) * (length + 1)); // allocate memory
    snprintf(res, length + 1, "%f", *((double *) args[0]->p_val)); // populate memory
  } else if (args[0]->type == TYPE_INT) {
    int length = snprintf(NULL, 0, "%i", *((int *) args[0]->p_val));
    res = malloc(sizeof(char) * (length + 1));
    snprintf(res, length + 1, "%i", *((int *) args[0]->p_val));
  } else {
    int length = snprintf(NULL, 0, "%s", *((char **) args[0]->p_val));
    res = malloc(sizeof(char) * (length + 1));
    snprintf(res, length + 1, "%s", *((char **) args[0]->p_val));
  }

  char **p_res = (char **) malloc(sizeof(char *));
  *p_res = res;

  return Generic_new(TYPE_STRING, p_res, 0);
}

// (float x)
// returns x as an float, if string, float, or int passed
Generic *StdLib_float(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING, TYPE_INT, TYPE_FLOAT};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "float");

  double *p_res = malloc(sizeof(double));

  if (args[0]->type == TYPE_STRING) {
    char *str = *((char **) args[0]->p_val);
    *p_res = atof(str);
  } else if (args[0]->type == TYPE_FLOAT) {
    double f = *((double *) args[0]->p_val);
    *p_res = f;
  } else {
    int i = *((int *) args[0]->p_val);
    *p_res = (double) i;
  }

  return Generic_new(TYPE_FLOAT, p_res, 0);
}

//  (format-int value base)
// Format integer to custom base (2=binary, 8=octal, 10=decimal, 16=hex)
Generic *StdLib_format_int(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_INT};
  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "format-int");

  enum Type allowedTypes2[] = {TYPE_INT};
  validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "format-int");

  int64_t value = *((int *) args[0]->p_val);
  int base = *((int *) args[1]->p_val);

  // Validate base
  if (base != 2 && base != 8 && base != 10 && base != 16) {
    printf("Runtime Error @ Line %i: format-int base must be 2, 8, 10, or 16, got %i.\n",
           lineNumber, base);
    exit(0);
  }

  // Format using number_parse module
  char *formatted = formatInteger(value, base);

  char **p_res = (char **) malloc(sizeof(char *));
  *p_res = formatted;

  return Generic_new(TYPE_STRING, p_res, 0);
}

//  (format-float value precision)
// Format float with custom decimal precision (0-17)
Generic *StdLib_format_float(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_FLOAT, TYPE_INT};
  validateType(allowedTypes1, 2, args[0]->type, 1, lineNumber, "format-float");

  enum Type allowedTypes2[] = {TYPE_INT};
  validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "format-float");

  double value;
  if (args[0]->type == TYPE_FLOAT) {
    value = *((double *) args[0]->p_val);
  } else {
    value = (double) *((int *) args[0]->p_val);
  }

  int precision = *((int *) args[1]->p_val);

  // Validate precision
  if (precision < 0 || precision > 17) {
    printf("Runtime Error @ Line %i: format-float precision must be 0-17, got %i.\n",
           lineNumber, precision);
    exit(0);
  }

  // Format using number_parse module
  char *formatted = formatFloat(value, precision);

  char **p_res = (char **) malloc(sizeof(char *));
  *p_res = formatted;

  return Generic_new(TYPE_STRING, p_res, 0);
}

// (type arg)
// returns a string with the type of arg
Generic *StdLib_type(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  
  // get type
  char *type = getTypeString(args[0]->type);
  
  // malloc memory for result and write
  char *res = malloc(sizeof(char) * (strlen(type) + 1));
  strcpy(res, type);
  res[strlen(type)] = '\0';

  // pointer to string
  char **p_res = (char **) malloc(sizeof(char *));
  *p_res = res;

  // return new generic
  return Generic_new(TYPE_STRING, p_res, 0);
}

/* list and string */
// (list args...)
// returns a new list with args as values
Generic *StdLib_list(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  return Generic_new(TYPE_LIST, List_new(args, length), 0);
}

// (length list)
// returns list length
Generic *StdLib_length(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  
  enum Type allowedTypes[] = {TYPE_LIST, TYPE_STRING};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "length");

  // create int
  int *p_res = (int *) malloc(sizeof(int));

  // get length and return
  if (args[0]->type == TYPE_LIST) *p_res = List_length((List *) args[0]->p_val);
  else *p_res = strlen(*((char **) args[0]->p_val));
  return Generic_new(TYPE_INT, p_res, 0);
}

// (join arg1 arg2 arg3 ...)
// returns args joined together
Generic *StdLib_join(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(2, length, lineNumber);

  // validate first type
  enum Type allowedTypes[] = {TYPE_LIST, TYPE_STRING};
  validateType(allowedTypes, 3, args[0]->type, 1, lineNumber, "join");

  // validate rest of types
  enum Type selectedType[] = {args[0]->type};
  for (int i = 1; i < length; i++) {
    validateType(selectedType, 1, args[i]->type, i + 1, lineNumber, "join");
  }
  
  if (args[0]->type == TYPE_STRING) {
    // string case
    // calculate size (starts at 1 for \0)
    int stringSize = 1;
    for (int i = 0; i < length; i++) stringSize += strlen(*((char **) args[i]->p_val));

    // malloc memory
    char *res = (char *) malloc(sizeof(char) * stringSize);

    // copy / concat
    strcpy(res, *((char **) args[0]->p_val));
    for (int i = 1; i < length; i++) strcat(res, *((char **) args[i]->p_val));

    // malloc pointer
    char **p_res = (char **) malloc(sizeof(char *));
    *p_res = res;

    return Generic_new(TYPE_STRING, p_res, 0);
  } else {
    // list case
    // create array of lists
    List *lists[length];
    
    // populate
    for (int i = 0; i < length; i++) lists[i] = ((List *) args[i]->p_val);

    // return new generic using List_join
    return Generic_new(TYPE_LIST, List_join(lists, length), 0);
  }
}

// (repeat string count)
// returns string repeated count times
Generic *StdLib_repeat(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  // Validate types
  enum Type allowedTypes1[] = {TYPE_STRING};
  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "repeat");

  enum Type allowedTypes2[] = {TYPE_INT};
  validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "repeat");

  char *str = *((char **) args[0]->p_val);
  int count = *((int *) args[1]->p_val);

  // Edge case: count <= 0 returns empty string
  if (count <= 0) {
    char *empty = (char *) malloc(sizeof(char));
    empty[0] = '\0';
    return Generic_new(TYPE_STRING, &empty, 0);
  }

  // Calculate total size needed
  int strLen = strlen(str);
  int totalSize = (strLen * count) + 1;  // +1 for \0

  // Allocate memory
  char *result = (char *) malloc(sizeof(char) * totalSize);
  result[0] = '\0';

  // Repeat string count times
  for (int i = 0; i < count; i++) {
    strcat(result, str);
  }

  return Generic_new(TYPE_STRING, &result, 0);
}

// (get list index1) or (get list index1 index2)
// returns item from list/string, or sublist/substring
Generic *StdLib_get(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 3, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_LIST, TYPE_STRING};
  validateType(allowedTypes1, 2, args[0]->type, 1, lineNumber, "get");

  enum Type allowedTypes2[] = {TYPE_INT};
  validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "get");
  if (length == 3) validateType(allowedTypes2, 1, args[2]->type, 3, lineNumber, "get");

  if (args[0]->type == TYPE_LIST) {
    int inputLength = List_length((List *) args[0]->p_val);
    validateRange(args[1]->p_val, 0, inputLength - 1, 2, lineNumber, "get");

    // single item from list
    if (length == 2) return List_get((List *) (args[0]->p_val), *((int *) args[1]->p_val));

    // multiple items from list
    else if (length == 3) {
      validateRange(args[2]->p_val, *((int *) args[1]->p_val) + 1, inputLength, 3, lineNumber, "get");

      return Generic_new(TYPE_LIST, List_sublist(
        (List *) (args[0]->p_val), 
        *((int *) args[1]->p_val), 
        *((int *) args[2]->p_val)
      ), 0);
    }

  } else if (args[0]->type == TYPE_STRING) {
    int inputLength = strlen(*((char **) args[0]->p_val));
    validateRange(args[1]->p_val, 0, inputLength - 1, 2, lineNumber, "get");

    if (length == 2) {
      // single item from string
      char *res = malloc(sizeof(char) * 2);
      res[0] = (*((char **) args[0]->p_val))[*((int *) args[1]->p_val)];
      res[1] = '\0';

      char **p_res = (char **) malloc(sizeof(char *));
      *p_res = res;

      return Generic_new(TYPE_STRING, p_res, 0);
    } else if (length == 3) {
      // mutliple items from string
      validateRange(args[2]->p_val, *((int *) args[1]->p_val) + 1, inputLength, 3, lineNumber, "get");

      // create substring
      int start = *((int *) args[1]->p_val);
      int end = *((int *) args[2]->p_val);

      char *res = malloc(sizeof(char) * (end - start + 1));
      strncpy(
        res, 
        &(*((char **) args[0]->p_val))[start], 
        end - start
      );
      
      res[end - start] = 0;

      // create pointer
      char **p_res = (char **) malloc(sizeof(char *));
      *p_res = res;

      // return
      return Generic_new(TYPE_STRING, p_res, 0);
    }
  }

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (insert list item index) or (insert list item)
// returns list or string, modified with item at index, if no index supplied, end is assumed
Generic *StdLib_insert(Scope *p_scope, Generic *args[], int length, int lineNumber) {

  // input validation
  validateArgCount(2, 3, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_LIST, TYPE_STRING};
  validateType(allowedTypes1, 2, args[0]->type, 1, lineNumber, "insert");

  enum Type allowedTypes2[] = {TYPE_STRING};
  if (args[0]->type == TYPE_STRING) 
    validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "insert");

  if (length == 3) {
    enum Type allowedTypes3[] = {TYPE_INT};
    validateType(allowedTypes3, 1, args[2]->type, 3, lineNumber, "insert");

    int inputLength = args[0]->type == TYPE_LIST 
      ? List_length((List *) args[0]->p_val)
      : strlen(*((char **) args[0]->p_val));
    validateRange(args[2]->p_val, 0, inputLength, 3, lineNumber, "insert");
  }

  if (args[0]->type == TYPE_LIST) {
    
    // list case
    if (length == 2) {
      return Generic_new(TYPE_LIST, List_insert(
        (List *) (args[0]->p_val), args[1], List_length((List *) (args[0]->p_val))
      ), 0);
    } else if (length == 3) {
      return Generic_new(TYPE_LIST, List_insert(
        (List *) (args[0]->p_val), args[1], *((int *) args[2]->p_val)
      ), 0); 
    }
  } else if (args[0]->type == TYPE_STRING) {

    // string case
    if (length == 2) {

      // when an index is not supplied, put simply acts like join
      return StdLib_join(p_scope, args, length, lineNumber);
    } else if (length == 3) {
      int stringSize = strlen(*((char **) args[0]->p_val)) + strlen(*((char **) args[1]->p_val)) + 1;

      // malloc memory
      char *res = (char *) malloc(sizeof(char) * stringSize);


      // copy / concat
      strncpy(res, *((char **) args[0]->p_val), *((int *) args[2]->p_val));
      res[*((int *) args[2]->p_val)] = '\0';

      strcat(res, *((char **) args[1]->p_val));
      strcat(res, &((*((char **) args[0]->p_val))[*((int *) args[2]->p_val)]));

      // pointer
      char **p_res = (char **) malloc(sizeof(char *));
      *p_res = res;

      // return
      return Generic_new(TYPE_STRING, p_res, 0);
    }
  }

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (set list item index)
// sets index to item
Generic *StdLib_set(Scope *p_scope, Generic *args[], int length, int lineNumber) {

  // input validation
  validateArgCount(3, 3, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_LIST, TYPE_STRING};
  validateType(allowedTypes1, 2, args[0]->type, 1, lineNumber, "set");

  enum Type allowedTypes2[] = {TYPE_STRING};
  if (args[0]->type == TYPE_STRING) 
    validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "set");

  enum Type allowedTypes3[] = {TYPE_INT};
  validateType(allowedTypes3, 1, args[2]->type, 3, lineNumber, "set");

  int inputLength = args[0]->type == TYPE_LIST 
    ? List_length((List *) args[0]->p_val)
    : strlen(*((char **) args[0]->p_val));
  validateRange(args[2]->p_val, 0, inputLength - 1, 3, lineNumber, "set");

  if (args[0]->type == TYPE_LIST) {
    // list case
    return Generic_new(TYPE_LIST, List_set(
      (List *) (args[0]->p_val), args[1], *((int *) args[2]->p_val)
    ), 0); 

  } else if (args[0]->type == TYPE_STRING) {
    char *target = *((char **) args[0]->p_val);
    char *item = *((char **) args[1]->p_val);
    int index = *((int *) args[2]->p_val);

    // string case
    // length of result
    int stringSize = index + (strlen(item) > strlen(target) - index ? strlen(item) : strlen(target) - index) + 1;

    // malloc memory
    char *res = (char *) malloc(sizeof(char) * stringSize);

    // copy / concat
    strncpy(res, target, index);
    res[index] = '\0';

    strcat(res, item);

    strncat(res, &(target[index]), stringSize - strlen(res) - 1);

    // pointer
    char **p_res = (char **) malloc(sizeof(char *));
    *p_res = res;

    // return
    return Generic_new(TYPE_STRING, p_res, 0);
  }

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (delete list index)
// deletes item from list and returns
Generic *StdLib_delete(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 3, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_LIST, TYPE_STRING};
  validateType(allowedTypes1, 2, args[0]->type, 1, lineNumber, "delete");

  enum Type allowedTypes2[] = {TYPE_INT};
  validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "delete");
  if (length == 3) validateType(allowedTypes2, 1, args[2]->type, 3, lineNumber, "delete");

  if (args[0]->type == TYPE_LIST) {
    int inputLength = List_length((List *) args[0]->p_val);
    validateRange(args[1]->p_val, 0, inputLength - 1, 2, lineNumber, "delete");

    // list case
    if (length == 2) {
      return Generic_new(TYPE_LIST, List_delete((List *) (args[0]->p_val), *((int *) args[1]->p_val)), 0);
    } else if (length == 3) {
      validateRange(args[2]->p_val, *((int *) args[1]->p_val) + 1, inputLength, 3, lineNumber, "delete");

      // case where we must delete multiple items
      return Generic_new(TYPE_LIST, List_deleteMultiple(
        (List *) (args[0]->p_val), *((int *) args[1]->p_val), *((int *) args[2]->p_val)
      ), 0);
    }

  } else {
    // string case
    int inputLength = strlen(*((char **) args[0]->p_val));
    validateRange(args[1]->p_val, 0, inputLength - 1, 2, lineNumber, "delete");

    char *target = *((char **) args[0]->p_val);
    int index1 = *((int *) args[1]->p_val);

    if (length == 2) {

      // length of result (no +1, as we are removing a charechter)
      int stringSize = strlen(target);

      // malloc memory
      char *res = (char *) malloc(sizeof(char) * stringSize);

      // copy / concat
      strncpy(res, target, index1);
      res[index1] = '\0';
      strncat(res, &(target[index1 + 1]), stringSize - strlen(res) - 1);

      // pointer
      char **p_res = (char **) malloc(sizeof(char *));
      *p_res = res;

      // return
      return Generic_new(TYPE_STRING, p_res, 0);
    } else if (length == 3) {
      // mutliple items from string
      validateRange(args[2]->p_val, *((int *) args[1]->p_val) + 1, inputLength, 3, lineNumber, "delete");

      int index2 = *((int *) args[2]->p_val);

      // length of result
      int stringSize = strlen(target) + 1 - (index2 - index1);

      // malloc memory
      char *res = (char *) malloc(sizeof(char) * stringSize);

      // copy / concat
      strncpy(res, target, index1);
      res[index1] = '\0';
      strncat(res, &(target[index2]), stringSize - strlen(res) - 1);

      // pointer
      char **p_res = (char **) malloc(sizeof(char *));
      *p_res = res;

      // return
      return Generic_new(TYPE_STRING, p_res, 0); 
    }
  }

  return Generic_new(TYPE_VOID, NULL, 0);
}

// (map list fn)
// applys fn to every item in list, returns list with results
Generic *StdLib_map(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_LIST};
  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "map");

  enum Type allowedTypes2[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes2, 3, args[1]->type, 2, lineNumber, "map");  //  3 types now

  // create copy of list
  Generic *res = Generic_copy(args[0]);
  List *p_list = (List *) (res->p_val);

  for (int i = 0; i < p_list->len; i += 1) {
    int *p_i = (int *) malloc(sizeof(int));
    *p_i = i;

    Generic *newArgs[] = {p_list->vals[i], Generic_new(TYPE_INT, p_i, 0)};
    p_list->vals[i] = applyFunc(args[1], p_scope, newArgs, 2, lineNumber);
  }

  return res;
}

// (reduce list fn acc)
// applys fn to every item in list, returns single reduced item
// acc is initial accumulator (assumed to be void if not supplied)
Generic *StdLib_reduce(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 3, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_LIST};
  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "reduce");

  enum Type allowedTypes2[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes2, 3, args[1]->type, 2, lineNumber, "reduce");  //  3 types now

  // TODO: check empty list case  
  
  // create accumulator
  Generic *p_acc;
  if (length == 3) {
    p_acc = Generic_copy(args[2]);
  } else {
    p_acc = Generic_new(TYPE_VOID, NULL, 0);
  }

  // Get list
  List *p_list = (List *) (args[0]->p_val);
 
  // loop on every item
  for (int i = 0; i < p_list->len; i += 1) {
    // create pointer for index, to create generic for args
    int *p_i = (int *) malloc(sizeof(int));
    *p_i = i;
    
    // apply function
    Generic *newArgs[] = {p_acc, Generic_copy(p_list->vals[i]), Generic_new(TYPE_INT, p_i, 0)};
    p_acc = applyFunc(args[1], p_scope, newArgs, 3, lineNumber);
  }

  return p_acc;
}

// (range n)
// returns list with range from 0 -> n - 1
Generic *StdLib_range(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_INT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "range");
  validateMin(args[0]->p_val, 0, 1, lineNumber, "range");

  // make list of arguments to pass to List_new
  int count = *((int *) args[0]->p_val);
  Generic *argList[count];
  
  for (int i = 0; i < count; i++) {
    int *p_i = (int *) malloc(sizeof(int));
    *p_i = i;
    argList[i] = Generic_new(TYPE_INT, p_i, 0);
  }

  Generic *res = Generic_new(TYPE_LIST, List_new(argList, count), 0);

  // free
  for (int i = 0; i < count; i++) {
    Generic_free(argList[i]);
  }

  return res;
}

// (find x item)
// returns index of item
Generic *StdLib_find(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes1[] = {TYPE_STRING, TYPE_LIST};
  validateType(allowedTypes1, 2, args[0]->type, 1, lineNumber, "find");

  if (args[0]->type == TYPE_STRING) {

    // string case
    enum Type allowedTypes2[] = {TYPE_STRING};
    validateType(allowedTypes2, 1, args[1]->type, 2, lineNumber, "find");

    // find pointer to substring
    char *p_sub = strstr(*((char **) args[0]->p_val), *((char **) args[1]->p_val));

    // return void if not found
    if (p_sub == NULL) return Generic_new(TYPE_VOID, NULL, 0);

    // get index and return
    int *p_index = malloc(sizeof(int)); 
    *p_index = p_sub - *((char **) args[0]->p_val);

    return Generic_new(TYPE_INT, p_index, 0);
  } else {
    // list case
    List *p_list = ((List *) args[0]->p_val);

    // for each item
    for (int i = 0; i < p_list->len; i += 1) {

      // if found, return index
      if (Generic_is(p_list->vals[i], args[1])) {
        int *p_index = (int *) malloc(sizeof(int));
        *p_index = i;
        return Generic_new(TYPE_INT, p_index, 0);
      }
    }

    // if not found return void
    return Generic_new(TYPE_VOID, NULL, 0);
  }
}

// ===== Algebraic Data Types (Variants) =====

// (variant tag values...)
// Build a tagged union value as a list: [tag: string, values: list]
Generic *StdLib_variant(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "variant");

  int valCount = length - 1;
  Generic **vals = NULL;
  if (valCount > 0) {
    vals = (Generic **) malloc(sizeof(Generic *) * valCount);
    for (int i = 0; i < valCount; i++) vals[i] = args[i + 1];
  }

  // values list
  List *valuesList = List_new(vals, valCount);
  Generic *valuesGen = Generic_new(TYPE_LIST, valuesList, 0);

  // variant structure: [tag, values]
  Generic *items[2];
  items[0] = args[0];
  items[1] = valuesGen;
  List *variantList = List_new(items, 2);

  // cleanup temporary generic (List_new made a deep copy)
  Generic_free(valuesGen);
  if (vals) free(vals);

  return Generic_new(TYPE_LIST, variantList, 0);
}

// (variant_tag v)
// Return the tag (string)
Generic *StdLib_variant_tag(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  enum Type allowed[] = {TYPE_LIST};
  validateType(allowed, 1, args[0]->type, 1, lineNumber, "variant_tag");

  List *lst = (List *) args[0]->p_val;
  if (lst->len < 1) {
    printf("Runtime Error @ Line %i: variant_tag expected non-empty variant.\n", lineNumber);
    exit(0);
  }
  Generic *tag = List_get(lst, 0);
  if (tag->type != TYPE_STRING) {
    printf("Runtime Error @ Line %i: variant_tag expected string tag.\n", lineNumber);
    exit(0);
  }
  return tag;
}

// (variant_values v)
// Return the values list
Generic *StdLib_variant_values(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  enum Type allowed[] = {TYPE_LIST};
  validateType(allowed, 1, args[0]->type, 1, lineNumber, "variant_values");

  List *lst = (List *) args[0]->p_val;
  if (lst->len < 2) {
    printf("Runtime Error @ Line %i: variant_values expected [tag, values] structure.\n", lineNumber);
    exit(0);
  }
  Generic *vals = List_get(lst, 1);
  if (vals->type != TYPE_LIST) {
    printf("Runtime Error @ Line %i: variant_values expected second element to be list.\n", lineNumber);
    exit(0);
  }
  return vals;
}

// (match v tag1 fn1 tag2 fn2 ... [defaultFn])
// Calls the matching function with unpacked variant values.
Generic *StdLib_match(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateMinArgCount(3, length, lineNumber);
  enum Type allowedVar[] = {TYPE_LIST};
  validateType(allowedVar, 1, args[0]->type, 1, lineNumber, "match");

  // Extract tag and values from variant
  List *lst = (List *) args[0]->p_val;
  if (lst->len < 2) {
    printf("Runtime Error @ Line %i: match expected variant [tag, values].\n", lineNumber);
    exit(0);
  }
  Generic *tag = List_get(lst, 0);
  if (tag->type != TYPE_STRING) {
    printf("Runtime Error @ Line %i: match expected string tag.\n", lineNumber);
    exit(0);
  }
  Generic *valsGen = List_get(lst, 1);
  if (valsGen->type != TYPE_LIST) {
    printf("Runtime Error @ Line %i: match expected list of values.\n", lineNumber);
    exit(0);
  }
  List *values = (List *) valsGen->p_val;

  // Determine if default function provided
  int hasDefault = ((length - 1) % 2 == 1) ? 1 : 0;
  int pairsEnd = length - hasDefault;

  // Attempt to match each tag/fn pair
  for (int i = 1; i < pairsEnd; i += 2) {
    if (args[i]->type != TYPE_STRING) {
      printf("Runtime Error @ Line %i: match expected string tag at argument #%i.\n", lineNumber, i + 1);
      exit(0);
    }
    //  Support closures in match
    if (args[i + 1]->type != TYPE_FUNCTION && args[i + 1]->type != TYPE_NATIVEFUNCTION && args[i + 1]->type != TYPE_BYTECODE_CLOSURE) {
      printf("Runtime Error @ Line %i: match expected function at argument #%i.\n", lineNumber, i + 2);
      exit(0);
    }

    if (strcmp(*((char **) args[i]->p_val), *((char **) tag->p_val)) == 0) {
      // Build call args from variant values
      int argcVals = values->len;
      Generic **callArgs = NULL;
      if (argcVals > 0) {
        callArgs = (Generic **) malloc(sizeof(Generic *) * argcVals);
        for (int k = 0; k < argcVals; k++) callArgs[k] = Generic_copy(values->vals[k]);
      }
      Generic *res = applyFunc(args[i + 1], p_scope, callArgs, argcVals, lineNumber);
      if (callArgs) {
        for (int k = 0; k < argcVals; k++) if (callArgs[k]->refCount == 0) Generic_free(callArgs[k]);
        free(callArgs);
      }
      Generic_free(tag);
      Generic_free(valsGen);
      return res;
    }
  }

  // Default case
  if (hasDefault) {
    Generic *fn = args[length - 1];
    //  Support closures in match default case
    if (fn->type != TYPE_FUNCTION && fn->type != TYPE_NATIVEFUNCTION && fn->type != TYPE_BYTECODE_CLOSURE) {
      printf("Runtime Error @ Line %i: match expected function for default case.\n", lineNumber);
      exit(0);
    }
    Generic *oneArg[1];
    oneArg[0] = Generic_copy(args[0]);
    Generic *res = applyFunc(fn, p_scope, oneArg, 1, lineNumber);
    if (oneArg[0]->refCount == 0) Generic_free(oneArg[0]);
    Generic_free(tag);
    Generic_free(valsGen);
    return res;
  }

  Generic_free(tag);
  Generic_free(valsGen);
  return Generic_new(TYPE_VOID, NULL, 0);
}

// ===== List Helper Functions (for recursive programming) =====

// (head lst)
// Returns first element of list, or void if empty
Generic *StdLib_head(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  enum Type allowedTypes[] = {TYPE_LIST};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "head");

  List *lst = (List *) args[0]->p_val;
  if (lst->len == 0) {
    return Generic_new(TYPE_VOID, NULL, 0);
  }
  // Return a copy to avoid reference issues
  return Generic_copy(lst->vals[0]);
}

// (tail lst)
// Returns rest of list (all but first element), or empty list if list has 0 or 1 elements
Generic *StdLib_tail(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  enum Type allowedTypes[] = {TYPE_LIST};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "tail");

  List *lst = (List *) args[0]->p_val;
  if (lst->len <= 1) {
    return Generic_new(TYPE_LIST, List_new(NULL, 0), 0);
  }
  return Generic_new(TYPE_LIST, List_sublist(lst, 1, lst->len), 0);
}

// (cons item lst)
// Prepends item to list, returns new list
Generic *StdLib_cons(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);
  enum Type allowedTypes[] = {TYPE_LIST};
  validateType(allowedTypes, 1, args[1]->type, 2, lineNumber, "cons");

  List *lst = (List *) args[1]->p_val;
  return Generic_new(TYPE_LIST, List_insert(lst, args[0], 0), 0);
}

// (empty? lst)
// Returns 1 if list is empty, 0 otherwise
Generic *StdLib_is_empty(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);
  enum Type allowedTypes[] = {TYPE_LIST};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "empty?");

  List *lst = (List *) args[0]->p_val;
  int *res = (int *) malloc(sizeof(int));
  *res = (lst->len == 0) ? 1 : 0;
  return Generic_new(TYPE_INT, res, 0);
}

/* Immutability Functions */

// (freeze name) - Makes an existing mutable binding immutable
Generic *StdLib_freeze(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_STRING};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "freeze");

  char *name = *((char **) args[0]->p_val);

  // Freeze the binding
  Scope_freeze(p_scope, name, lineNumber);

  return Generic_new(TYPE_VOID, NULL, 0);
}

/* Function Composition Helpers */

// (pipe value fn1 fn2 fn3 ...)
// Threads a value through a sequence of functions left-to-right
// Each function receives the result of the previous function as its single argument
// Returns the final result after all transformations
Generic *StdLib_pipe(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  // Validate minimum argument count (at least value + one function)
  if (length < 2) {
    printf("Runtime Error @ Line %i: pipe requires at least a value and one function.\n", lineNumber);
    exit(0);
  }

  // Start with the initial value (first argument)
  // Use Generic_copy for first value to avoid modifying original
  Generic *result = Generic_copy(args[0]);

  // Apply each function in sequence (left-to-right)
  for (int i = 1; i < length; i++) {
    //  Validate that argument is a function (including closures)
    if (args[i]->type != TYPE_FUNCTION && args[i]->type != TYPE_NATIVEFUNCTION && args[i]->type != TYPE_BYTECODE_CLOSURE) {
      printf("Runtime Error @ Line %i: pipe argument %i must be a function.\n",
             lineNumber, i + 1);
      exit(0);
    }

    // Prepare arguments for function call
    // Note: applyFunc will increment/decrement refCount automatically
    Generic *funcArgs[1];
    funcArgs[0] = result;

    // Apply the function to the current result
    // applyFunc will free result if refCount reaches 0
    Generic *new_result = applyFunc(args[i], p_scope, funcArgs, 1, lineNumber);

    // Update result for next iteration
    result = new_result;
  }

  return result;
}

// (partial function arg1 arg2 ...)
// Creates a partially applied function with fixed arguments
// Returns a list [function, arg1, arg2, ...] to be used with (call ...)
//
// Usage:
//   add_five = (partial add 5)
//   result = (call add_five 10)  // Calls (add 5 10)
Generic *StdLib_partial(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  // Validate: at least function + 1 fixed argument
  if (length < 2) {
    printf("Runtime Error @ Line %i: partial requires a function and at least one argument.\n", lineNumber);
    exit(0);
  }

  //  Validate first argument is a function (including closures)
  if (args[0]->type != TYPE_FUNCTION && args[0]->type != TYPE_NATIVEFUNCTION && args[0]->type != TYPE_BYTECODE_CLOSURE) {
    printf("Runtime Error @ Line %i: partial first argument must be a function.\n", lineNumber);
    exit(0);
  }

  // Create a list to store: [function, arg1, arg2, ...]
  Generic **partialData = (Generic **) malloc(sizeof(Generic *) * length);

  // Store function and all fixed arguments
  for (int i = 0; i < length; i++) {
    partialData[i] = Generic_copy(args[i]);
  }

  List *partialList = List_new(partialData, length);
  free(partialData);

  return Generic_new(TYPE_LIST, partialList, 0);
}

// (call partial_fn new_args...)
// Applies a partial function created by (partial ...)
// Combines fixed args with new args and calls the original function
//
// Example:
//   add_five = (partial add 5)
//   (call add_five 10)  // Returns 15
Generic *StdLib_call(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length < 1) {
    printf("Runtime Error @ Line %i: call requires at least a partial function.\n", lineNumber);
    exit(0);
  }

  // First arg should be the partial (a list)
  if (args[0]->type != TYPE_LIST) {
    printf("Runtime Error @ Line %i: call first argument must be a partial function (got list from partial).\n", lineNumber);
    exit(0);
  }

  List *partial = (List *) args[0]->p_val;

  if (partial->len < 2) {
    printf("Runtime Error @ Line %i: Invalid partial function - must have [function, ...fixed_args].\n", lineNumber);
    exit(0);
  }

  // Extract original function (first element)
  Generic *original_fn = partial->vals[0];

  //  Validate it's actually a function (including closures)
  if (original_fn->type != TYPE_FUNCTION && original_fn->type != TYPE_NATIVEFUNCTION && original_fn->type != TYPE_BYTECODE_CLOSURE) {
    printf("Runtime Error @ Line %i: First element of partial must be a function.\n", lineNumber);
    exit(0);
  }

  // Count fixed args (all elements after function)
  int fixed_count = partial->len - 1;

  // Count new args (all args after partial itself)
  int new_count = length - 1;

  // Total arg count for original function
  int total_count = fixed_count + new_count;

  // Combine fixed args + new args
  Generic **combined_args = (Generic **) malloc(sizeof(Generic *) * total_count);

  // Copy fixed args from partial
  // Increment refCount to protect from applyFunc freeing them
  for (int i = 0; i < fixed_count; i++) {
    combined_args[i] = partial->vals[i + 1];
    combined_args[i]->refCount++;
  }

  // Copy new args (these are already protected by caller)
  for (int i = 0; i < new_count; i++) {
    combined_args[fixed_count + i] = args[i + 1];
  }

  // Protect the function from being freed by applyFunc (line 136 in applyFunc)
  original_fn->refCount++;

  // Apply original function with combined args
  Generic *result = applyFunc(original_fn, p_scope, combined_args, total_count, lineNumber);

  // Restore function refCount (applyFunc may have decremented it)
  original_fn->refCount--;

  free(combined_args);
  return result;
}

// (thread-first value form1 form2 ...)
// Threads value through forms, inserting it as FIRST argument
// Each form must be a list: [fn, arg2, arg3, ...]
// Example: (thread-first 5 (list add 3) (list multiply 2)) = 16
Generic *StdLib_thread_first(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length < 2) {
    printf("Runtime Error @ Line %i: thread-first requires at least a value and one form.\n", lineNumber);
    exit(0);
  }

  // Start with the initial value
  Generic *result = Generic_copy(args[0]);

  // Process each form
  for (int i = 1; i < length; i++) {
    Generic *form = args[i];

    // Form must be a list: [fn, arg2, arg3, ...]
    if (form->type != TYPE_LIST) {
      printf("Runtime Error @ Line %i: thread-first form must be a list [function, args...].\n", lineNumber);
      exit(0);
    }

    List *form_list = (List *) form->p_val;

    if (form_list->len < 1) {
      printf("Runtime Error @ Line %i: thread-first form must have at least a function.\n", lineNumber);
      exit(0);
    }

    Generic *fn = form_list->vals[0];

    //  Support closures in thread-first
    if (fn->type != TYPE_FUNCTION && fn->type != TYPE_NATIVEFUNCTION && fn->type != TYPE_BYTECODE_CLOSURE) {
      printf("Runtime Error @ Line %i: thread-first form's first element must be a function.\n", lineNumber);
      exit(0);
    }

    // Build arg list: [result, arg2, arg3, ...]
    int arg_count = form_list->len; // result + (form_list->len - 1) additional args
    Generic **funcArgs = (Generic **) malloc(sizeof(Generic *) * arg_count);

    funcArgs[0] = result; // Result goes FIRST
    for (int j = 1; j < form_list->len; j++) {
      funcArgs[j] = form_list->vals[j];
      funcArgs[j]->refCount++;
    }

    fn->refCount++;
    Generic *new_result = applyFunc(fn, p_scope, funcArgs, arg_count, lineNumber);
    fn->refCount--;

    free(funcArgs);
    result = new_result;
  }

  return result;
}

// (thread-last value form1 form2 ...)
// Threads value through forms, inserting it as LAST argument
// Each form must be a list: [fn, arg1, arg2, ...]
// Example: (thread-last nums (list map double_fn) (list reduce sum_fn 0))
Generic *StdLib_thread_last(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length < 2) {
    printf("Runtime Error @ Line %i: thread-last requires at least a value and one form.\n", lineNumber);
    exit(0);
  }

  // Start with the initial value
  Generic *result = Generic_copy(args[0]);

  // Process each form
  for (int i = 1; i < length; i++) {
    Generic *form = args[i];

    // Form must be a list: [fn, arg1, arg2, ...]
    if (form->type != TYPE_LIST) {
      printf("Runtime Error @ Line %i: thread-last form must be a list [function, args...].\n", lineNumber);
      exit(0);
    }

    List *form_list = (List *) form->p_val;

    if (form_list->len < 1) {
      printf("Runtime Error @ Line %i: thread-last form must have at least a function.\n", lineNumber);
      exit(0);
    }

    Generic *fn = form_list->vals[0];

    //  Support closures in thread-last
    if (fn->type != TYPE_FUNCTION && fn->type != TYPE_NATIVEFUNCTION && fn->type != TYPE_BYTECODE_CLOSURE) {
      printf("Runtime Error @ Line %i: thread-last form's first element must be a function.\n", lineNumber);
      exit(0);
    }

    // Build arg list: [arg1, arg2, ..., result]
    int arg_count = form_list->len; // (form_list->len - 1) + result
    Generic **funcArgs = (Generic **) malloc(sizeof(Generic *) * arg_count);

    // Copy form args first
    for (int j = 1; j < form_list->len; j++) {
      funcArgs[j - 1] = form_list->vals[j];
      funcArgs[j - 1]->refCount++;
    }

    // Result goes LAST
    funcArgs[arg_count - 1] = result;

    fn->refCount++;
    Generic *new_result = applyFunc(fn, p_scope, funcArgs, arg_count, lineNumber);
    fn->refCount--;

    free(funcArgs);
    result = new_result;
  }

  return result;
}

// Collection-last wrappers for thread-last compatibility
// These accept collection as LAST argument instead of FIRST

// (map-last callback collection)
// Same as map but with arguments reversed for thread-last
Generic *StdLib_map_last(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: map-last requires exactly 2 arguments (callback, collection).\n", lineNumber);
    exit(0);
  }

  // Reverse the arguments: map expects (collection, callback)
  Generic *reversed[2];
  reversed[0] = args[1];  // collection (was last, now first)
  reversed[1] = args[0];  // callback (was first, now last)

  // Call the original map function
  return StdLib_map(p_scope, reversed, 2, lineNumber);
}

// (reduce-last callback init collection)
// Same as reduce but with collection as last argument for thread-last
Generic *StdLib_reduce_last(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 3) {
    printf("Runtime Error @ Line %i: reduce-last requires exactly 3 arguments (callback, init, collection).\n", lineNumber);
    exit(0);
  }

  // Reorder arguments: reduce expects (collection, callback, init)
  Generic *reordered[3];
  reordered[0] = args[2];  // collection (was last, now first)
  reordered[1] = args[0];  // callback (was first, stays second)
  reordered[2] = args[1];  // init (was second, now third)

  // Call the original reduce function
  return StdLib_reduce(p_scope, reordered, 3, lineNumber);
}

// (filter collection callback)
// Standard filter with collection-first signature (like map, reduce)
// Callback signature: {item index -> boolean}
Generic *StdLib_filter(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: filter requires exactly 2 arguments (collection, callback).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_LIST};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "filter");

  List *input = (List *) args[0]->p_val;

  // Build result list
  Generic **filtered = (Generic **) malloc(sizeof(Generic *) * input->len);
  int count = 0;

  for (int i = 0; i < input->len; i++) {
    int *p_i = (int *) malloc(sizeof(int));
    *p_i = i;

    // Protect input value from being freed by applyFunc
    input->vals[i]->refCount++;

    Generic *funcArgs[] = {input->vals[i], Generic_new(TYPE_INT, p_i, 0)};

    args[1]->refCount++;
    Generic *result = applyFunc(args[1], p_scope, funcArgs, 2, lineNumber);
    args[1]->refCount--;

    // Restore refCount
    input->vals[i]->refCount--;

    // If result is truthy (not 0), include this element
    if (result->type == TYPE_INT && *((int *) result->p_val) != 0) {
      filtered[count++] = Generic_copy(input->vals[i]);
    }

    if (result->refCount == 0) Generic_free(result);
  }

  List *resultList = List_new(filtered, count);
  free(filtered);

  return Generic_new(TYPE_LIST, resultList, 0);
}

// (filter-last callback collection)
// Collection-last wrapper for thread-last compatibility
Generic *StdLib_filter_last(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: filter-last requires exactly 2 arguments (callback, collection).\n", lineNumber);
    exit(0);
  }

  // Reverse arguments: filter expects (collection, callback)
  Generic *reversed[2];
  reversed[0] = args[1];  // collection (was last, now first)
  reversed[1] = args[0];  // callback (was first, now last)

  return StdLib_filter(p_scope, reversed, 2, lineNumber);
}

// ============================================================================
// Dictionary Functions (Native Hash Table Implementation - O(1) lookups)
// ============================================================================
// Dicts use native TYPE_DICT with hash table for efficient operations
// Example: (dict "name" "Ada" "age" 37) -> {name: Ada, age: 37}

// (dict key1 val1 key2 val2 ...)
// Creates a dictionary from alternating keys and values
Generic *StdLib_dict(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length % 2 != 0) {
    printf("Runtime Error @ Line %i: dict requires an even number of arguments (key-value pairs).\n", lineNumber);
    exit(0);
  }

  // Create new dict with appropriate capacity
  int pair_count = length / 2;
  int initial_capacity = pair_count > 0 ? pair_count * 2 : 16;
  Dict *dict = Dict_new(initial_capacity);

  // Insert all key-value pairs using in-place mutation (efficient building)
  for (int i = 0; i < pair_count; i++) {
    Generic *key = args[i * 2];
    Generic *value = args[i * 2 + 1];

    Dict_set_inplace(dict, key, value);
  }

  return Generic_new(TYPE_DICT, dict, 0);
}

// (dict_get dict key)
// Returns the value associated with key, or void if not found
// O(1) average-case lookup time
Generic *StdLib_dict_get(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: dict_get requires exactly 2 arguments (dict, key).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_get");

  Dict *dict = (Dict *) args[0]->p_val;
  Generic *key = args[1];

  Generic *value = Dict_get(dict, key);

  if (value == NULL) {
    return Generic_new(TYPE_VOID, NULL, 0);
  }

  return Generic_copy(value);
}

// (dict_set dict key value)
// Returns a new dict with the key-value pair added/updated (immutable)
// O(n) for immutable copy, O(1) for lookup
Generic *StdLib_dict_set(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 3) {
    printf("Runtime Error @ Line %i: dict_set requires exactly 3 arguments (dict, key, value).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_set");

  Dict *dict = (Dict *) args[0]->p_val;
  Generic *key = args[1];
  Generic *value = args[2];

  Dict *new_dict = Dict_set(dict, key, value);

  return Generic_new(TYPE_DICT, new_dict, 0);
}

// (dict_keys dict)
// Returns a list of all keys in the dict
// O(n) where n is the number of entries
Generic *StdLib_dict_keys(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 1) {
    printf("Runtime Error @ Line %i: dict_keys requires exactly 1 argument (dict).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_keys");

  Dict *dict = (Dict *) args[0]->p_val;
  int count;
  Generic **keys_arr = Dict_keys(dict, &count);

  // Copy keys for return
  Generic **keys_copy = (Generic **) malloc(sizeof(Generic *) * count);
  for (int i = 0; i < count; i++) {
    keys_copy[i] = Generic_copy(keys_arr[i]);
  }

  List *result = List_new(keys_copy, count);
  free(keys_copy);
  free(keys_arr);

  return Generic_new(TYPE_LIST, result, 0);
}

// (dict_values dict)
// Returns a list of all values in the dict
// O(n) where n is the number of entries
Generic *StdLib_dict_values(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 1) {
    printf("Runtime Error @ Line %i: dict_values requires exactly 1 argument (dict).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_values");

  Dict *dict = (Dict *) args[0]->p_val;
  int count;
  Generic **values_arr = Dict_values(dict, &count);

  // Copy values for return
  Generic **values_copy = (Generic **) malloc(sizeof(Generic *) * count);
  for (int i = 0; i < count; i++) {
    values_copy[i] = Generic_copy(values_arr[i]);
  }

  List *result = List_new(values_copy, count);
  free(values_copy);
  free(values_arr);

  return Generic_new(TYPE_LIST, result, 0);
}

// (dict_has dict key)
// Returns 1 if key exists, 0 otherwise
// O(1) average-case lookup time
Generic *StdLib_dict_has(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: dict_has requires exactly 2 arguments (dict, key).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_has");

  Dict *dict = (Dict *) args[0]->p_val;
  Generic *key = args[1];

  int has = Dict_has(dict, key);

  int *result = (int *) malloc(sizeof(int));
  *result = has;
  return Generic_new(TYPE_INT, result, 0);
}

// (dict_merge dict1 dict2)
// Returns a new dict with all keys from both dicts (dict2 values override dict1)
// O(n + m) where n, m are sizes of dict1, dict2
Generic *StdLib_dict_merge(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: dict_merge requires exactly 2 arguments (dict1, dict2).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_merge");
  validateType(allowedTypes, 1, args[1]->type, 2, lineNumber, "dict_merge");

  Dict *dict1 = (Dict *) args[0]->p_val;
  Dict *dict2 = (Dict *) args[1]->p_val;

  Dict *merged = Dict_merge(dict1, dict2);

  return Generic_new(TYPE_DICT, merged, 0);
}

// (dict_remove dict key)
// Returns a new dict with the key removed (immutable)
// O(n) for immutable copy
Generic *StdLib_dict_remove(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: dict_remove requires exactly 2 arguments (dict, key).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes[] = {TYPE_DICT};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "dict_remove");

  Dict *dict = (Dict *) args[0]->p_val;
  Generic *key = args[1];

  Dict *new_dict = Dict_remove(dict, key);

  return Generic_new(TYPE_DICT, new_dict, 0);
}

// (dict_map dict fn)
// Returns a new dict with all values transformed by fn
// fn signature: {key value -> new_value}
// O(n) where n is the number of entries
Generic *StdLib_dict_map(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: dict_map requires exactly 2 arguments (dict, function).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes1[] = {TYPE_DICT};
  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "dict_map");

  enum Type allowedTypes2[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes2, 3, args[1]->type, 2, lineNumber, "dict_map");  //  3 types now

  Dict *dict = (Dict *) args[0]->p_val;
  Generic *map_fn = args[1];

  // Create new empty dict
  Dict *new_dict = Dict_new(dict->capacity);

  // Iterate through all entries
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      Generic *key = entry->key;
      Generic *value = entry->value;

      // Call fn with key and value
      key->refCount++;
      value->refCount++;

      Generic *fn_args[] = {key, value};

      map_fn->refCount++;
      Generic *new_value = applyFunc(map_fn, p_scope, fn_args, 2, lineNumber);
      map_fn->refCount--;

      key->refCount--;
      value->refCount--;

      // Insert into new dict using in-place mutation
      Dict_set_inplace(new_dict, key, new_value);

      if (new_value->refCount == 0) Generic_free(new_value);

      entry = entry->next;
    }
  }

  return Generic_new(TYPE_DICT, new_dict, 0);
}

// (dict_filter dict fn)
// Returns a new dict containing only pairs where fn returns truthy
// fn signature: {key value -> boolean}
// O(n) where n is the number of entries
Generic *StdLib_dict_filter(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  if (length != 2) {
    printf("Runtime Error @ Line %i: dict_filter requires exactly 2 arguments (dict, function).\n", lineNumber);
    exit(0);
  }

  enum Type allowedTypes1[] = {TYPE_DICT};
  validateType(allowedTypes1, 1, args[0]->type, 1, lineNumber, "dict_filter");

  enum Type allowedTypes2[] = {TYPE_FUNCTION, TYPE_NATIVEFUNCTION, TYPE_BYTECODE_CLOSURE, TYPE_BYTECODE_CLOSURE};  //  Support closures
  validateType(allowedTypes2, 3, args[1]->type, 2, lineNumber, "dict_filter");  //  3 types now

  Dict *dict = (Dict *) args[0]->p_val;
  Generic *filter_fn = args[1];

  // Create new empty dict
  Dict *new_dict = Dict_new(dict->capacity);

  // Iterate through all entries
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      Generic *key = entry->key;
      Generic *value = entry->value;

      // Call fn with key and value
      key->refCount++;
      value->refCount++;

      Generic *fn_args[] = {key, value};

      filter_fn->refCount++;
      Generic *result = applyFunc(filter_fn, p_scope, fn_args, 2, lineNumber);
      filter_fn->refCount--;

      key->refCount--;
      value->refCount--;

      // If result is truthy, keep this pair
      bool should_keep = false;
      if (result->type == TYPE_INT && *((int *) result->p_val) != 0) {
        should_keep = true;
      }

      if (should_keep) {
        Dict_set_inplace(new_dict, key, value);
      }

      if (result->refCount == 0) Generic_free(result);

      entry = entry->next;
    }
  }

  return Generic_new(TYPE_DICT, new_dict, 0);
}

//  Mutable References

// (ref value) -> creates a mutable reference
Generic *StdLib_ref(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  // Create new Ref containing the value
  Generic *value_copy = Generic_copy(args[0]);
  value_copy->refCount++;  // Increment because Ref will hold it

  Ref *ref = Ref_new(value_copy);

  return Generic_new(TYPE_REF, ref, 0);
}

// (deref ref) -> gets current value from reference
Generic *StdLib_deref(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(1, 1, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_REF};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "deref");

  Ref *ref = (Ref *) args[0]->p_val;
  return Ref_get(ref);  // Returns a copy for safety
}

// (set! ref new-value) -> updates reference with new value
Generic *StdLib_set_ref(Scope *p_scope, Generic *args[], int length, int lineNumber) {
  validateArgCount(2, 2, length, lineNumber);

  enum Type allowedTypes[] = {TYPE_REF};
  validateType(allowedTypes, 1, args[0]->type, 1, lineNumber, "set!");

  Ref *ref = (Ref *) args[0]->p_val;

  // Copy the new value and set it
  Generic *new_value = Generic_copy(args[1]);
  new_value->refCount++;  // Increment because Ref will hold it

  Ref_set(ref, new_value);

  return Generic_new(TYPE_VOID, NULL, 0);
}

// creates a new global scope
Scope *newGlobal(int argc, char *argv[]) {

  // initialize random number generator for (random)
  srand(time(NULL) + clock());

  // create global scope
  Scope *p_global = Scope_new(NULL);
  Generic *args[argc];

  // for each arg
  for (int i = 0; i < argc; i++) {
    char **p_val = (char **) malloc(sizeof(char *));

    // create string
    char *val = (char *) malloc(sizeof(char) * (strlen(argv[i]) + 1));
    strcpy(val, argv[i]);
    *p_val = val;

    // add to args
    args[i] = Generic_new(TYPE_STRING, p_val, 0);
  }
  
  // add arguments and arguments count
  Scope_set(p_global, "arguments", Generic_new(TYPE_LIST, List_new(args, argc), 0), -1);

  // free arguments memory (as List_new does a copy)
  for (int i = 0; i < argc; i++) {
    Generic_free(args[i]);
    args[i] = NULL;
  }

  // add void
  Scope_set(p_global, "void", Generic_new(TYPE_VOID, NULL, 0), -1);

  // populate global scope with stdlib functions
  /* IO */
  Scope_set(p_global, "print", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_print, 0), -1);
  Scope_set(p_global, "println", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_println, 0), -1);
  Scope_set(p_global, "input", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_input, 0), -1);
  Scope_set(p_global, "rows", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_rows, 0), -1);
  Scope_set(p_global, "columns", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_columns, 0), -1);
  Scope_set(p_global, "read_file", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_read_file, 0), -1);
  Scope_set(p_global, "write_file", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_write_file, 0), -1);
  Scope_set(p_global, "file_exists", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_file_exists, 0), -1);
  Scope_set(p_global, "append_file", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_append_file, 0), -1);

  /*  Advanced file operations */
  Scope_set(p_global, "read_binary", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_read_binary, 0), -1);
  Scope_set(p_global, "write_binary", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_write_binary, 0), -1);
  Scope_set(p_global, "list_files", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_list_files, 0), -1);
  Scope_set(p_global, "create_dir", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_create_dir, 0), -1);
  Scope_set(p_global, "dir_exists", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dir_exists, 0), -1);
  Scope_set(p_global, "remove_dir", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_remove_dir, 0), -1);
  Scope_set(p_global, "file_size", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_file_size, 0), -1);
  Scope_set(p_global, "file_mtime", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_file_mtime, 0), -1);
  Scope_set(p_global, "is_directory", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_is_directory, 0), -1);

  Scope_set(p_global, "event", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_event, 0), -1);
  Scope_set(p_global, "use", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_use, 0), -1);
  Scope_set(p_global, "use_as", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_use_as, 0), -1);
  Scope_set(p_global, "use_with", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_use_with, 0), -1);
  Scope_set(p_global, "shell", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_shell, 0), -1);

  /* comparisions */
  Scope_set(p_global, "is", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_is, 0), -1);
  Scope_set(p_global, "less_than", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_less_than, 0), -1);
  Scope_set(p_global, "greater_than", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_greater_than, 0), -1);

  /* logical operators */
  Scope_set(p_global, "not", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_not, 0), -1);
  Scope_set(p_global, "and", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_and, 0), -1);
  Scope_set(p_global, "or", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_or, 0), -1);

  /* arithmetic */
  Scope_set(p_global, "add", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_add, 0), -1);
  Scope_set(p_global, "subtract", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_subtract, 0), -1);
  Scope_set(p_global, "divide", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_divide, 0), -1);
  Scope_set(p_global, "multiply", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_multiply, 0), -1);
  Scope_set(p_global, "remainder", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_remainder, 0), -1);
  Scope_set(p_global, "power", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_power, 0), -1);
  Scope_set(p_global, "random", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_random, 0), -1);
  Scope_set(p_global, "floor", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_floor, 0), -1);
  Scope_set(p_global, "ceil", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_ceil, 0), -1);
  Scope_set(p_global, "round", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_round, 0), -1);
  Scope_set(p_global, "abs", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_abs, 0), -1);
  Scope_set(p_global, "min", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_min, 0), -1);
  Scope_set(p_global, "max", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_max, 0), -1);
  Scope_set(p_global, "sqrt", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_sqrt, 0), -1);
  Scope_set(p_global, "random_int", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_random_int, 0), -1);
  Scope_set(p_global, "random_range", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_random_range, 0), -1);
  Scope_set(p_global, "random_seed", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_random_seed, 0), -1);

  /* control */
  Scope_set(p_global, "loop", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_loop, 0), -1);
  Scope_set(p_global, "until", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_until, 0), -1);
  Scope_set(p_global, "if", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_if, 0), -1);
  Scope_set(p_global, "wait", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_wait, 0), -1);
  Scope_set(p_global, "time", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_time, 0), -1);

  /* error handling */
  Scope_set(p_global, "try", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_try, 0), -1);
  Scope_set(p_global, "catch", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_catch, 0), -1);
  Scope_set(p_global, "error", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_error, 0), -1);

  /* types */
  Scope_set(p_global, "integer", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_integer, 0), -1);
  Scope_set(p_global, "string", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_string, 0), -1);
  Scope_set(p_global, "float", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_float, 0), -1);

  //  Custom formatting functions
  Scope_set(p_global, "format-int", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_format_int, 0), -1);
  Scope_set(p_global, "format-float", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_format_float, 0), -1);
  Scope_set(p_global, "type", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_type, 0), -1);

  /* list and string */
  Scope_set(p_global, "list", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_list, 0), -1);
  Scope_set(p_global, "length", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_length, 0), -1);
  Scope_set(p_global, "join", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_join, 0), -1);
  Scope_set(p_global, "repeat", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_repeat, 0), -1);
  Scope_set(p_global, "get", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_get, 0), -1);
  Scope_set(p_global, "insert", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_insert, 0), -1);
  Scope_set(p_global, "set", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_set, 0), -1);
  Scope_set(p_global, "delete", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_delete, 0), -1);
  Scope_set(p_global, "map", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_map, 0), -1);
  Scope_set(p_global, "reduce", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_reduce, 0), -1);
  Scope_set(p_global, "filter", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_filter, 0), -1);
  Scope_set(p_global, "range", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_range, 0), -1);
  Scope_set(p_global, "find", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_find, 0), -1);

  /* list helpers (for recursive programming) */
  Scope_set(p_global, "head", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_head, 0), -1);
  Scope_set(p_global, "tail", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_tail, 0), -1);
  Scope_set(p_global, "cons", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_cons, 0), -1);
  Scope_set(p_global, "empty?", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_is_empty, 0), -1);

  /* algebraic data types (variants) */
  Scope_set(p_global, "variant", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_variant, 0), -1);
  Scope_set(p_global, "variant_tag", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_variant_tag, 0), -1);
  Scope_set(p_global, "variant_values", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_variant_values, 0), -1);
  Scope_set(p_global, "match", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_match, 0), -1);

  /* immutability */
  Scope_set(p_global, "freeze", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_freeze, 0), -1);

  /* function composition helpers */
  Scope_set(p_global, "pipe", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_pipe, 0), -1);
  Scope_set(p_global, "partial", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_partial, 0), -1);
  Scope_set(p_global, "call", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_call, 0), -1);
  Scope_set(p_global, "thread-first", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_thread_first, 0), -1);
  Scope_set(p_global, "thread-last", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_thread_last, 0), -1);

  /* collection-last wrappers for thread-last compatibility */
  Scope_set(p_global, "map-last", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_map_last, 0), -1);
  Scope_set(p_global, "reduce-last", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_reduce_last, 0), -1);
  Scope_set(p_global, "filter-last", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_filter_last, 0), -1);

  /* dictionary functions (list-of-pairs implementation) */
  Scope_set(p_global, "dict", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict, 0), -1);
  Scope_set(p_global, "dict_get", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_get, 0), -1);
  Scope_set(p_global, "dict_set", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_set, 0), -1);
  Scope_set(p_global, "dict_keys", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_keys, 0), -1);
  Scope_set(p_global, "dict_values", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_values, 0), -1);
  Scope_set(p_global, "dict_has", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_has, 0), -1);
  Scope_set(p_global, "dict_merge", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_merge, 0), -1);
  Scope_set(p_global, "dict_remove", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_remove, 0), -1);
  Scope_set(p_global, "dict_map", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_map, 0), -1);
  Scope_set(p_global, "dict_filter", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_dict_filter, 0), -1);

  /* mutable references () */
  Scope_set(p_global, "ref", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_ref, 0), -1);
  Scope_set(p_global, "deref", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_deref, 0), -1);
  Scope_set(p_global, "set!", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_set_ref, 0), -1);

  return p_global;
}

// ============================================================================
//  Runtime Helpers for LLVM List Literals
// ============================================================================

// Helper: Box an i64 value into Generic*
Generic *franz_box_int(int64_t value) {
  // fprintf(stderr, "[BOX INT] Boxing value: %lld\n", (long long)value);
  int *p_val = (int *)malloc(sizeof(int));
  *p_val = (int)value;
  Generic *result = Generic_new(TYPE_INT, p_val, 0);
  // fprintf(stderr, "[BOX INT] Created Generic* at %p, type=%d\n", result, result->type);
  return result;
}

// Helper: Box a double value into Generic*
Generic *franz_box_float(double value) {
  // fprintf(stderr, "[BOX FLOAT] Boxing value: %f\n", value);
  double *p_val = (double *)malloc(sizeof(double));
  *p_val = value;
  Generic *result = Generic_new(TYPE_FLOAT, p_val, 0);
  // fprintf(stderr, "[BOX FLOAT] Created Generic* at %p, type=%d\n", result, result->type);
  return result;
}

// Helper: Box a string (i8*) value into Generic*
Generic *franz_box_string(char *value) {
  // fprintf(stderr, "[BOX STRING] Boxing value: %p = '%s'\n", value, value ? value : "(null)");
  // Franz stores strings as char** (pointer to pointer)
  if (!value) {
    // fprintf(stderr, "[FRANZ_BOX_STRING ERROR] NULL input!\n");
    // Return empty string Generic*
    char **p_val = (char **)malloc(sizeof(char *));
    *p_val = (char *)malloc(1);
    (*p_val)[0] = '\0';
    Generic *result = Generic_new(TYPE_STRING, p_val, 0);
    // fprintf(stderr, "[BOX STRING] Created Generic* at %p, type=%d\n", result, result->type);
    return result;
  }

  char **p_val = (char **)malloc(sizeof(char *));
  *p_val = (char *)malloc(strlen(value) + 1);
  strcpy(*p_val, value);
  Generic *result = Generic_new(TYPE_STRING, p_val, 0);
  // fprintf(stderr, "[BOX STRING] Created Generic* at %p, type=%d, value='%s'\n", result, result->type, *p_val);
  return result;
}

//  Smart pointer boxing - checks if already Generic*, otherwise boxes as string
// This handles polymorphic closures returning POINTER type (could be string OR Generic* list)
Generic *franz_box_pointer_smart(void *ptr) {
  fprintf(stderr, "[BOX POINTER SMART] Checking pointer: %p\n", ptr);

  uintptr_t addr = (uintptr_t)ptr;
  if (addr == 0) {
    fprintf(stderr, "[BOX POINTER SMART] NULL pointer, treating as int 0 payload\n");
    return franz_box_int(0);
  }

  if (addr < 4096) {
    // Pointer is actually an int payload (from inttoptr), box as int safely
    fprintf(stderr, "[BOX POINTER SMART] Small address (%zu) treated as int payload\n", (size_t)addr);
    return franz_box_int((int64_t)addr);
  }

  // Try to interpret as Generic* and check if it has a valid type
  // Generic* has 'type' field as first member (int)
  Generic *potential_generic = (Generic *)ptr;

  // Check if type field is in valid range (0-10 covers all Franz types)
  // If it looks like a Generic*, return it as-is
  // Otherwise, treat as raw string pointer and box it
  if (potential_generic->type >= TYPE_INT && potential_generic->type <= TYPE_REF) {
    fprintf(stderr, "[BOX POINTER SMART] Detected Generic* (type=%d), returning as-is\n", potential_generic->type);
    return potential_generic;
  } else {
    fprintf(stderr, "[BOX POINTER SMART] Not Generic* (type=%d at first 4 bytes), boxing as string\n", potential_generic->type);
    fprintf(stderr, "[BOX POINTER SMART] String content: %.20s\n", (char*)ptr);
    return franz_box_string((char *)ptr);
  }
}
#ifndef CLOSURE_RETURN_INT
#define CLOSURE_RETURN_INT 0
#define CLOSURE_RETURN_FLOAT 1
#define CLOSURE_RETURN_POINTER 2
#define CLOSURE_RETURN_CLOSURE 3
#define CLOSURE_RETURN_VOID 4
#endif

//  Helper for LLVM equality - box closure parameters using runtime tags
Generic *franz_box_param_tag(int64_t rawValue, int tag) {
  switch (tag) {
    case CLOSURE_RETURN_INT:
      return franz_box_int(rawValue);
    case CLOSURE_RETURN_FLOAT: {
      double value = 0.0;
      memcpy(&value, &rawValue, sizeof(double));
      return franz_box_float(value);
    }
    case CLOSURE_RETURN_POINTER: {
      void *ptr = (void *)(intptr_t) rawValue;
      return franz_box_pointer_smart(ptr);
    }
    case CLOSURE_RETURN_CLOSURE: {
      void *ptr = (void *)(intptr_t) rawValue;
      return franz_box_closure(ptr);
    }
    case CLOSURE_RETURN_VOID:
      return franz_box_string("void");
    default:
      return franz_box_string("");
  }
}

//  Unbox Generic* to get closure i64 (for nested closures)
// Used in factory patterns where closures return other closures
// Input: Generic* (i64 cast to Generic*)
// Output: Raw closure i64 (from Generic->p_value)
int64_t franz_generic_to_closure_ptr(int64_t generic_i64) {
  fprintf(stderr, "[UNBOX CLOSURE] Unboxing Generic* %lld to closure i64\n", (long long)generic_i64);

  // Cast i64 back to Generic*
  Generic *generic = (Generic *)(intptr_t)generic_i64;

  if (!generic) {
    fprintf(stderr, "[UNBOX CLOSURE ERROR] NULL Generic*!\n");
    return 0;
  }

  fprintf(stderr, "[UNBOX CLOSURE] Generic type = %d (should be TYPE_CLOSURE)\n", generic->type);

  // Extract the closure i64 from p_val
  // The p_val contains the raw closure struct (cast to void*)
  int64_t closure_i64 = (int64_t)(intptr_t)(generic->p_val);

  fprintf(stderr, "[UNBOX CLOSURE] Extracted closure i64: %lld\n", (long long)closure_i64);

  return closure_i64;
}

// Helper: Box a list (Generic* list) - already boxed, just wrap
Generic *franz_box_list(Generic *list) {
  // List is already a Generic*, just return it
  return list;
}

// Helper: Box a closure (Closure* from LLVM) into Generic*
// CRITICAL: LLVM closures are raw Closure* structs, NOT Generic* pointers
// This function wraps them properly for runtime use
Generic *franz_box_closure(void *closure_ptr) {
  // Closure* is already allocated by LLVM, just wrap it in Generic*
  // NOTE: TYPE_BYTECODE_CLOSURE signals this is an LLVM closure, not runtime closure
  return Generic_new(TYPE_BYTECODE_CLOSURE, closure_ptr, 0);
}

// LLVM Closure struct layout: { i8* funcPtr, i8* envPtr, i32 returnTypeTag }
typedef struct LLVMClosure {
  void *funcPtr;
  void *envPtr;
  int returnTypeTag;  // 0=int, 1=float, 2=pointer
} LLVMClosure;

// Helper: Call an LLVM closure from runtime
// LLVM closures have function signature: result (*)(env, arg1, arg2, ...)
Generic *franz_call_llvm_closure(Generic *closure_gen, Generic *args[], int argCount, int lineNumber) {
  fprintf(stderr, "[RUNTIME DEBUG] franz_call_llvm_closure called with %d args\n", argCount);

  if (closure_gen->type != TYPE_BYTECODE_CLOSURE) {
    fprintf(stderr, "Runtime Error @ Line %d: Expected LLVM closure, got type %d\n", lineNumber, closure_gen->type);
    exit(1);
  }

  // Extract LLVM closure struct
  LLVMClosure *llvm_closure = (LLVMClosure *)closure_gen->p_val;
  fprintf(stderr, "[RUNTIME DEBUG] LLVM closure: funcPtr=%p, envPtr=%p, returnTypeTag=%d\n",
          llvm_closure->funcPtr, llvm_closure->envPtr, llvm_closure->returnTypeTag);

  // Function signature depends on whether it's a closure or regular function:
  // - Closure: result (*)(env, arg1, arg2, ...)
  // - Regular function: result (*)(arg1, arg2, ...)

  if (argCount == 2) {
    // Two-argument function: (key, value) -> result
    //  FIX: LLVM closures need UNBOXED primitive values, NOT Generic* pointers!
    // - For primitives (INT, FLOAT): unbox to get actual value
    // - For complex types (STRING, LIST, DICT): pass as Generic* pointer

    int64_t key_i64;
    if (args[0]->type == TYPE_INT) {
      key_i64 = (int64_t)(*((int *)args[0]->p_val));
    } else if (args[0]->type == TYPE_FLOAT) {
      double fval = *((double *)args[0]->p_val);
      key_i64 = *((int64_t *)&fval);  // Bitcast double to i64
    } else {
      key_i64 = (int64_t)args[0];  // Pass Generic* as pointer
    }

    int64_t val_i64;
    if (args[1]->type == TYPE_INT) {
      val_i64 = (int64_t)(*((int *)args[1]->p_val));
    } else if (args[1]->type == TYPE_FLOAT) {
      double fval = *((double *)args[1]->p_val);
      val_i64 = *((int64_t *)&fval);  // Bitcast double to i64
    } else {
      val_i64 = (int64_t)args[1];  // Pass Generic* as pointer
    }

    fprintf(stderr, "[ARG UNBOX DEBUG] args[0]=0x%llx (Generic*), key_i64=%lld (0x%llx)\n",
            (unsigned long long)args[0], key_i64, (unsigned long long)key_i64);
    fprintf(stderr, "[ARG UNBOX DEBUG] args[1]=0x%llx (Generic*), val_i64=%lld (0x%llx)\n",
            (unsigned long long)args[1], val_i64, (unsigned long long)val_i64);

    int64_t result;

    // CRITICAL FIX: LLVM closures expect tagged parameters (value + type tag)
    // Signature: (env, param1_val, param1_tag, param2_val, param2_tag) -> result
    int32_t key_tag = (int32_t)args[0]->type;  // Get Franz type enum
    int32_t val_tag = (int32_t)args[1]->type;

    if (llvm_closure->envPtr != NULL) {
      // CLOSURE: Has captured environment - call with env as first parameter
      int64_t env_i64 = (int64_t)llvm_closure->envPtr;
      fprintf(stderr, "[RUNTIME DEBUG] Calling CLOSURE with env_i64=%lld, key_i64=%lld, key_tag=%d, val_i64=%lld, val_tag=%d\n",
              env_i64, key_i64, key_tag, val_i64, val_tag);

      int64_t (*func)(int64_t, int64_t, int32_t, int64_t, int32_t) = (int64_t (*)(int64_t, int64_t, int32_t, int64_t, int32_t))llvm_closure->funcPtr;
      result = func(env_i64, key_i64, key_tag, val_i64, val_tag);
    } else {
      // REGULAR FUNCTION: No environment - call directly with args as i64 + tags
      fprintf(stderr, "[RUNTIME DEBUG] Calling REGULAR FUNCTION with key_i64=%lld, key_tag=%d, val_i64=%lld, val_tag=%d\n",
              key_i64, key_tag, val_i64, val_tag);

      int64_t (*func)(int64_t, int32_t, int64_t, int32_t) = (int64_t (*)(int64_t, int32_t, int64_t, int32_t))llvm_closure->funcPtr;
      result = func(key_i64, key_tag, val_i64, val_tag);
    }

    fprintf(stderr, "[RUNTIME DEBUG] LLVM function returned: %lld (0x%llx)\n", result, (unsigned long long)result);

    // CRITICAL: LLVM arithmetic operations return RAW primitive values, not Generic* pointers
    // We need to BOX the result based on returnTypeTag
    // - returnTypeTag=0: INT - box as Generic* with TYPE_INT
    // - returnTypeTag=1: FLOAT - box as Generic* with TYPE_FLOAT
    // - returnTypeTag=2: POINTER - already a Generic* pointer

    if (llvm_closure->returnTypeTag == 0) {
      // INT - box the raw integer
      fprintf(stderr, "[RESULT DEBUG] Boxing INT result: %lld\n", result);
      return franz_box_int(result);
    } else if (llvm_closure->returnTypeTag == 1) {
      // FLOAT - reinterpret i64 as double, then box
      double fval = *((double *)&result);
      fprintf(stderr, "[RESULT DEBUG] Boxing FLOAT result: %f\n", fval);
      return franz_box_float(fval);
    } else {
      // POINTER - already a Generic* pointer cast to i64
      Generic *result_generic = (Generic *)result;
      fprintf(stderr, "[RESULT DEBUG] Returning POINTER result: %p\n", result_generic);
      return result_generic;
    }
  } else if (argCount == 1) {
    // Single-argument function: (value) -> result
    //  FIX: LLVM closures need UNBOXED primitive values, NOT Generic* pointers!
    int64_t arg_i64;
    if (args[0]->type == TYPE_INT) {
      arg_i64 = (int64_t)(*((int *)args[0]->p_val));
    } else if (args[0]->type == TYPE_FLOAT) {
      double fval = *((double *)args[0]->p_val);
      arg_i64 = *((int64_t *)&fval);  // Bitcast double to i64
    } else {
      arg_i64 = (int64_t)args[0];  // Pass Generic* as pointer
    }

    fprintf(stderr, "[ARG UNBOX DEBUG] args[0]=0x%llx (Generic*), arg_i64=%lld (0x%llx)\n",
            (unsigned long long)args[0], arg_i64, (unsigned long long)arg_i64);

    int64_t result;

    if (llvm_closure->envPtr != NULL) {
      // CLOSURE: Has captured environment - call with env as first parameter
      int64_t env_i64 = (int64_t)llvm_closure->envPtr;
      fprintf(stderr, "[RUNTIME DEBUG] Calling CLOSURE with env_i64=%lld, arg_i64=%lld\n",
              env_i64, arg_i64);

      int64_t (*func)(int64_t, int64_t) = (int64_t (*)(int64_t, int64_t))llvm_closure->funcPtr;
      result = func(env_i64, arg_i64);
    } else {
      // REGULAR FUNCTION: No environment - call directly with arg as i64
      fprintf(stderr, "[RUNTIME DEBUG] Calling REGULAR FUNCTION with arg_i64=%lld\n", arg_i64);

      int64_t (*func)(int64_t) = (int64_t (*)(int64_t))llvm_closure->funcPtr;
      result = func(arg_i64);
    }

    fprintf(stderr, "[RUNTIME DEBUG] LLVM function returned: %lld (0x%llx)\n", result, (unsigned long long)result);

    // CRITICAL: Box the result based on returnTypeTag
    // - returnTypeTag=0: INT - box as Generic* with TYPE_INT
    // - returnTypeTag=1: FLOAT - box as Generic* with TYPE_FLOAT
    // - returnTypeTag=2: POINTER - already a Generic* pointer

    if (llvm_closure->returnTypeTag == 0) {
      // INT - box the raw integer
      fprintf(stderr, "[RESULT DEBUG] Boxing INT result: %lld\n", result);
      return franz_box_int(result);
    } else if (llvm_closure->returnTypeTag == 1) {
      // FLOAT - reinterpret i64 as double, then box
      double fval = *((double *)&result);
      fprintf(stderr, "[RESULT DEBUG] Boxing FLOAT result: %f\n", fval);
      return franz_box_float(fval);
    } else {
      // POINTER - already a Generic* pointer cast to i64
      Generic *result_generic = (Generic *)result;
      fprintf(stderr, "[RESULT DEBUG] Returning POINTER result: %p\n", result_generic);
      return result_generic;
    }
  } else if (argCount == 3) {
    // Three-argument function: (arg1, arg2, arg3) -> result
    // Used by map2 for (elem1, elem2, index)
    //  LLVM closures need UNBOXED primitive values, NOT Generic* pointers!

    int64_t arg1_i64;
    if (args[0]->type == TYPE_INT) {
      arg1_i64 = (int64_t)(*((int *)args[0]->p_val));
    } else if (args[0]->type == TYPE_FLOAT) {
      double fval = *((double *)args[0]->p_val);
      arg1_i64 = *((int64_t *)&fval);  // Bitcast double to i64
    } else {
      arg1_i64 = (int64_t)args[0];  // Pass Generic* as pointer
    }

    int64_t arg2_i64;
    if (args[1]->type == TYPE_INT) {
      arg2_i64 = (int64_t)(*((int *)args[1]->p_val));
    } else if (args[1]->type == TYPE_FLOAT) {
      double fval = *((double *)args[1]->p_val);
      arg2_i64 = *((int64_t *)&fval);  // Bitcast double to i64
    } else {
      arg2_i64 = (int64_t)args[1];  // Pass Generic* as pointer
    }

    int64_t arg3_i64;
    if (args[2]->type == TYPE_INT) {
      arg3_i64 = (int64_t)(*((int *)args[2]->p_val));
    } else if (args[2]->type == TYPE_FLOAT) {
      double fval = *((double *)args[2]->p_val);
      arg3_i64 = *((int64_t *)&fval);  // Bitcast double to i64
    } else {
      arg3_i64 = (int64_t)args[2];  // Pass Generic* as pointer
    }

    fprintf(stderr, "[ARG UNBOX DEBUG] args[0]=0x%llx, arg1_i64=%lld (0x%llx)\n",
            (unsigned long long)args[0], arg1_i64, (unsigned long long)arg1_i64);
    fprintf(stderr, "[ARG UNBOX DEBUG] args[1]=0x%llx, arg2_i64=%lld (0x%llx)\n",
            (unsigned long long)args[1], arg2_i64, (unsigned long long)arg2_i64);
    fprintf(stderr, "[ARG UNBOX DEBUG] args[2]=0x%llx, arg3_i64=%lld (0x%llx)\n",
            (unsigned long long)args[2], arg3_i64, (unsigned long long)arg3_i64);

    int64_t result;

    // CRITICAL FIX: LLVM closures expect tagged parameters
    int32_t arg1_tag = (int32_t)args[0]->type;
    int32_t arg2_tag = (int32_t)args[1]->type;
    int32_t arg3_tag = (int32_t)args[2]->type;

    if (llvm_closure->envPtr != NULL) {
      // CLOSURE: Has captured environment
      int64_t env_i64 = (int64_t)llvm_closure->envPtr;
      fprintf(stderr, "[RUNTIME DEBUG] Calling CLOSURE with env_i64=%lld, arg1=%lld (tag=%d), arg2=%lld (tag=%d), arg3=%lld (tag=%d)\n",
              env_i64, arg1_i64, arg1_tag, arg2_i64, arg2_tag, arg3_i64, arg3_tag);

      int64_t (*func)(int64_t, int64_t, int32_t, int64_t, int32_t, int64_t, int32_t) =
        (int64_t (*)(int64_t, int64_t, int32_t, int64_t, int32_t, int64_t, int32_t))llvm_closure->funcPtr;
      result = func(env_i64, arg1_i64, arg1_tag, arg2_i64, arg2_tag, arg3_i64, arg3_tag);
    } else {
      // REGULAR FUNCTION: No environment
      fprintf(stderr, "[RUNTIME DEBUG] Calling REGULAR FUNCTION with arg1=%lld (tag=%d), arg2=%lld (tag=%d), arg3=%lld (tag=%d)\n",
              arg1_i64, arg1_tag, arg2_i64, arg2_tag, arg3_i64, arg3_tag);

      int64_t (*func)(int64_t, int32_t, int64_t, int32_t, int64_t, int32_t) =
        (int64_t (*)(int64_t, int32_t, int64_t, int32_t, int64_t, int32_t))llvm_closure->funcPtr;
      result = func(arg1_i64, arg1_tag, arg2_i64, arg2_tag, arg3_i64, arg3_tag);
    }

    fprintf(stderr, "[RUNTIME DEBUG] LLVM function returned: %lld (0x%llx)\n", result, (unsigned long long)result);

    // Box the result based on returnTypeTag
    if (llvm_closure->returnTypeTag == 0) {
      // INT - box the raw integer
      fprintf(stderr, "[RESULT DEBUG] Boxing INT result: %lld\n", result);
      return franz_box_int(result);
    } else if (llvm_closure->returnTypeTag == 1) {
      // FLOAT - reinterpret i64 as double, then box
      double fval = *((double *)&result);
      fprintf(stderr, "[RESULT DEBUG] Boxing FLOAT result: %f\n", fval);
      return franz_box_float(fval);
    } else {
      // POINTER - already a Generic* pointer cast to i64
      Generic *result_generic = (Generic *)result;
      fprintf(stderr, "[RESULT DEBUG] Returning POINTER result: %p\n", result_generic);
      return result_generic;
    }
  }

  fprintf(stderr, "Runtime Error @ Line %d: franz_call_llvm_closure only supports 1, 2, or 3-argument closures currently\n", lineNumber);
  exit(1);
}

// Helper: Create list from array of Generic* pointers
// This is called from LLVM-generated code with boxed elements
Generic *franz_list_new(Generic **elements, int length) {
  return Generic_new(TYPE_LIST, List_new(elements, length), 0);
}

// ============================================================================
//  Industry-Standard List Operations (Rust-like implementation)
// ============================================================================

// Helper: Get first element of list (head/car)
Generic *franz_list_head(Generic *list) {
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error: head requires a list argument\n");
    exit(1);
  }
  List *l = (List *)list->p_val;
  if (l->len == 0) {
    fprintf(stderr, "Runtime Error: head called on empty list\n");
    exit(1);
  }
  return l->vals[0];
}

// Helper: Get rest of list (tail/cdr)
Generic *franz_list_tail(Generic *list) {
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error: tail requires a list argument\n");
    exit(1);
  }
  List *l = (List *)list->p_val;
  if (l->len == 0) {
    fprintf(stderr, "Runtime Error: tail called on empty list\n");
    exit(1);
  }
  // Create new list with elements [1..len)
  Generic **newElements = (Generic **)malloc(sizeof(Generic *) * (l->len - 1));
  for (int i = 1; i < l->len; i++) {
    newElements[i - 1] = l->vals[i];
  }
  return franz_list_new(newElements, l->len - 1);
}

// Helper: Prepend element to list (cons)
Generic *franz_list_cons(Generic *elem, Generic *list) {
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error: cons requires a list as second argument\n");
    exit(1);
  }
  List *l = (List *)list->p_val;
  // Create new list with elem prepended
  Generic **newElements = (Generic **)malloc(sizeof(Generic *) * (l->len + 1));
  newElements[0] = elem;
  for (int i = 0; i < l->len; i++) {
    newElements[i + 1] = l->vals[i];
  }
  return franz_list_new(newElements, l->len + 1);
}

// Helper: Check if list is empty
int64_t franz_list_is_empty(Generic *list) {
  if (!list || list->type != TYPE_LIST) {
    return 0;  // Not a list, return false
  }
  List *l = (List *)list->p_val;
  return (l->len == 0) ? 1 : 0;
}

// Helper: Get length of list
int64_t franz_list_length(Generic *list) {
  if (!list) {
    fprintf(stderr, "Runtime Error: length requires a list or string argument\n");
    exit(1);
  }
  if (list->type == TYPE_LIST) {
    List *l = (List *)list->p_val;
    return l->len;
  }
  if (list->type == TYPE_STRING) {
    char **p_str = (char **)list->p_val;
    return (int64_t)strlen(*p_str);
  }
  fprintf(stderr, "Runtime Error: length requires a list or string argument\n");
  exit(1);
}

// Helper: Get element at index (0-indexed)
Generic *franz_list_nth(Generic *list, int64_t index) {
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error: nth requires a list as first argument\n");
    exit(1);
  }
  List *l = (List *)list->p_val;
  if (index < 0 || index >= l->len) {
    fprintf(stderr, "Runtime Error: list index %lld out of bounds (length %d)\n",
            (long long)index, l->len);
    exit(1);
  }
  return l->vals[index];
}

// Helper: Check if Generic* is a list
int64_t franz_is_list(Generic *value) {
  if (!value) return 0;
  return (value->type == TYPE_LIST) ? 1 : 0;
}

// ============================================================================
// LLVM Filter Implementation
// ============================================================================

/**
 * franz_llvm_filter - Runtime helper for LLVM-compiled filter
 *
 * Filters a list based on a predicate closure.
 * The predicate receives (element, index) and returns boolean.
 *
 * @param list Generic* - The list to filter (TYPE_LIST)
 * @param predicate Generic* - The predicate closure (TYPE_BYTECODE_CLOSURE)
 * @param lineNumber int - Line number for error reporting
 * @return Generic* - New filtered list
 */
Generic *franz_llvm_filter(Generic *list, Generic *predicate, int lineNumber) {
  fprintf(stderr, "[LLVM FILTER] Called with list type=%d, predicate type=%d\n",
          list ? list->type : -1, predicate ? predicate->type : -1);

  // Validate list argument
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error @ Line %d: filter requires a list as first argument\n", lineNumber);
    exit(1);
  }

  // Validate predicate argument
  if (!predicate || predicate->type != TYPE_BYTECODE_CLOSURE) {
    fprintf(stderr, "Runtime Error @ Line %d: filter requires an LLVM closure as second argument (got type %d)\n",
            lineNumber, predicate ? predicate->type : -1);
    exit(1);
  }

  List *input = (List *)list->p_val;
  fprintf(stderr, "[LLVM FILTER] Processing list with %d elements\n", input->len);

  // Build result list - allocate for worst case (all elements pass)
  Generic **filtered = (Generic **)malloc(sizeof(Generic *) * input->len);
  int count = 0;

  // Iterate through list elements
  for (int i = 0; i < input->len; i++) {
    fprintf(stderr, "[LLVM FILTER] Processing element %d\n", i);

    // Prepare arguments for predicate: (element, index)
    Generic *elem = input->vals[i];
    Generic *index_gen = franz_box_int(i);

    Generic *predicateArgs[] = { elem, index_gen };

    // Call the predicate closure with (element, index)
    fprintf(stderr, "[LLVM FILTER] Calling predicate for element %d\n", i);
    Generic *result = franz_call_llvm_closure(predicate, predicateArgs, 2, lineNumber);
    fprintf(stderr, "[LLVM FILTER] Predicate returned type=%d\n", result ? result->type : -1);

    // Check if result is truthy (non-zero for integers)
    int is_truthy = 0;
    if (result) {
      if (result->type == TYPE_INT) {
        int result_val = *((int *)result->p_val);
        is_truthy = (result_val != 0);
        fprintf(stderr, "[LLVM FILTER] Integer result: %d (truthy=%d)\n", result_val, is_truthy);
      } else {
        // Non-integer truthy values (strings, lists, etc. are truthy if non-null)
        is_truthy = 1;
      }
    }

    // Include element if predicate returned truthy value
    if (is_truthy) {
      fprintf(stderr, "[LLVM FILTER] Including element %d in result\n", i);
      filtered[count++] = Generic_copy(elem);
    } else {
      fprintf(stderr, "[LLVM FILTER] Excluding element %d from result\n", i);
    }

    // Clean up index Generic
    if (index_gen->refCount == 0) {
      Generic_free(index_gen);
    }

    // Clean up result if not referenced
    if (result && result->refCount == 0) {
      Generic_free(result);
    }
  }

  fprintf(stderr, "[LLVM FILTER] Filtered %d elements down to %d\n", input->len, count);

  // Create result list with only the filtered elements
  List *resultList = List_new(filtered, count);
  free(filtered);

  return Generic_new(TYPE_LIST, resultList, 0);
}

// ============================================================================
// LLVM Map Implementation
// ============================================================================

/**
 * franz_llvm_map - Runtime helper for LLVM-compiled map
 *
 * Transforms a list by applying a callback closure to each element.
 * The callback receives (element, index) and returns the transformed value.
 *
 * @param list Generic* - The list to map over (TYPE_LIST)
 * @param callback Generic* - The callback closure (TYPE_BYTECODE_CLOSURE)
 * @param lineNumber int - Line number for error reporting
 * @return Generic* - New transformed list
 */
Generic *franz_llvm_map(Generic *list, Generic *callback, int lineNumber) {
  // Validate list argument
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error @ Line %d: map requires a list as first argument\n", lineNumber);
    exit(1);
  }

  // Validate callback argument
  if (!callback || callback->type != TYPE_BYTECODE_CLOSURE) {
    fprintf(stderr, "Runtime Error @ Line %d: map requires an LLVM closure as second argument (got type %d)\n",
            lineNumber, callback ? callback->type : -1);
    exit(1);
  }

  List *input = (List *)list->p_val;
  // Build result list - allocate for all elements
  Generic **mapped = (Generic **)malloc(sizeof(Generic *) * input->len);

  // Iterate through list elements
  for (int i = 0; i < input->len; i++) {
    // Prepare arguments for callback: (element, index)
    Generic *elem = input->vals[i];
    Generic *index_gen = franz_box_int(i);

    Generic *callbackArgs[] = { elem, index_gen };

    // Call the callback closure with (element, index)
    Generic *result = franz_call_llvm_closure(callback, callbackArgs, 2, lineNumber);

    // Store the transformed value
    if (!result) {
      fprintf(stderr, "Runtime Error @ Line %d: map callback must return a value for element %d\n", lineNumber, i);
      exit(1);
    }

    mapped[i] = Generic_copy(result);

    // Clean up index Generic
    if (index_gen->refCount == 0) {
      Generic_free(index_gen);
    }

    // Clean up result if not referenced
    if (result && result->refCount == 0) {
      Generic_free(result);
    }
  }

  // Create result list with all transformed elements
  List *resultList = List_new(mapped, input->len);
  free(mapped);

  return Generic_new(TYPE_LIST, resultList, 0);
}

// ============================================================================
// LLVM Map2 Implementation (Multi-List Map)
// ============================================================================

/**
 * franz_llvm_map2 - Runtime helper for LLVM-compiled map2 (zip-map)
 *
 * Transforms two lists by applying a callback closure to corresponding elements.
 * The callback receives (elem1, elem2, index) and returns the combined value.
 *
 * @param list1 Generic* - The first list (TYPE_LIST)
 * @param list2 Generic* - The second list (TYPE_LIST)
 * @param callback Generic* - The callback closure (TYPE_BYTECODE_CLOSURE)
 * @param lineNumber int - Line number for error reporting
 * @return Generic* - New list with combined elements (length = min(len1, len2))
 */
Generic *franz_llvm_map2(Generic *list1, Generic *list2, Generic *callback, int lineNumber) {
  // Validate list1 argument
  if (!list1 || list1->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error @ Line %d: map2 requires a list as first argument\n", lineNumber);
    exit(1);
  }

  // Validate list2 argument
  if (!list2 || list2->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error @ Line %d: map2 requires a list as second argument\n", lineNumber);
    exit(1);
  }

  // Validate callback argument
  if (!callback || callback->type != TYPE_BYTECODE_CLOSURE) {
    fprintf(stderr, "Runtime Error @ Line %d: map2 requires an LLVM closure as third argument (got type %d)\n",
            lineNumber, callback ? callback->type : -1);
    exit(1);
  }

  List *input1 = (List *)list1->p_val;
  List *input2 = (List *)list2->p_val;

  // Result length is minimum of both lists (like zip)
  int resultLen = (input1->len < input2->len) ? input1->len : input2->len;

  // Build result list
  Generic **mapped = (Generic **)malloc(sizeof(Generic *) * resultLen);

  // Iterate through list elements
  for (int i = 0; i < resultLen; i++) {
    // Prepare arguments for callback: (element1, element2, index)
    Generic *elem1 = input1->vals[i];
    Generic *elem2 = input2->vals[i];
    Generic *index_gen = franz_box_int(i);

    Generic *callbackArgs[] = { elem1, elem2, index_gen };

    // Call the callback closure with (element1, element2, index)
    Generic *result = franz_call_llvm_closure(callback, callbackArgs, 3, lineNumber);

    // Store the transformed value
    if (!result) {
      fprintf(stderr, "Runtime Error @ Line %d: map2 callback must return a value for element %d\n", lineNumber, i);
      exit(1);
    }

    mapped[i] = Generic_copy(result);

    // Clean up index Generic
    if (index_gen->refCount == 0) {
      Generic_free(index_gen);
    }

    // Clean up result if not referenced
    if (result && result->refCount == 0) {
      Generic_free(result);
    }
  }

  // Create result list with all combined elements
  List *resultList = List_new(mapped, resultLen);
  free(mapped);

  return Generic_new(TYPE_LIST, resultList, 0);
}

// ============================================================================
//  Generic* Runtime Helpers for LLVM
// ============================================================================

// franz_generic_get_type(Generic*) -> i32 (type enum value)
int32_t franz_generic_get_type(Generic *g) {
  if (!g) return -1;
  return (int32_t)g->type;
}

// franz_generic_get_pval(Generic*) -> void* (p_val field)
void *franz_generic_get_pval(Generic *g) {
  if (!g) return NULL;
  return g->p_val;
}

// ============================================================================
// LLVM Reduce Implementation
// ============================================================================

/**
 * franz_llvm_reduce - Runtime helper for LLVM-compiled reduce
 *
 * Reduces a list to a single value by applying a callback closure to each element.
 * The callback receives (accumulator, element, index) and returns new accumulator.
 *
 * @param list Generic* - The list to reduce (TYPE_LIST)
 * @param callback Generic* - The callback closure (TYPE_BYTECODE_CLOSURE)
 * @param initial Generic* - Initial accumulator value (or NULL for void)
 * @param lineNumber int - Line number for error reporting
 * @return Generic* - Final accumulated value
 */
Generic *franz_llvm_reduce(Generic *list, Generic *callback, Generic *initial, int lineNumber) {
  // Validate list argument
  if (!list || list->type != TYPE_LIST) {
    fprintf(stderr, "Runtime Error @ Line %d: reduce requires a list as first argument\n", lineNumber);
    exit(1);
  }

  // Validate callback argument
  if (!callback || callback->type != TYPE_BYTECODE_CLOSURE) {
    fprintf(stderr, "Runtime Error @ Line %d: reduce requires an LLVM closure as second argument (got type %d)\n",
            lineNumber, callback ? callback->type : -1);
    exit(1);
  }

  List *input = (List *)list->p_val;

  // Initialize accumulator
  Generic *acc;
  if (initial) {
    acc = Generic_copy(initial);
  } else {
    // No initial value - use void
    acc = Generic_new(TYPE_VOID, NULL, 0);
  }

  // Iterate through list elements
  for (int i = 0; i < input->len; i++) {
    // Prepare arguments for callback: (accumulator, element, index)
    Generic *elem = input->vals[i];
    Generic *index_gen = franz_box_int(i);

    Generic *callbackArgs[] = { acc, elem, index_gen };

    // Call the callback closure with (acc, element, index)
    Generic *result = franz_call_llvm_closure(callback, callbackArgs, 3, lineNumber);

    // Clean up old accumulator
    if (acc && acc->refCount == 0) {
      Generic_free(acc);
    }

    // Clean up index Generic
    if (index_gen->refCount == 0) {
      Generic_free(index_gen);
    }

    // Update accumulator with result
    acc = result;
  }

  return acc;
}

/**
 *  LLVM Mutable References Runtime Helpers
 *
 * Implements runtime support for ref/deref/set! in LLVM native compilation:
 * - franz_llvm_create_ref: Create mutable reference
 * - franz_llvm_deref: Read current value
 * - franz_llvm_set_ref: Update reference
 *
 * Pattern: OCaml ref cells, Scheme boxes, Rust RefCell
 * Performance: C-level speed (10x faster than interpreter)
 */

/**
 * franz_llvm_create_ref(Generic* value, int lineNumber) -> Generic* (TYPE_REF)
 *
 * Creates a new mutable reference containing the initial value.
 * Returns Generic* with TYPE_REF.
 *
 * @param value Generic* - Initial value to store
 * @param lineNumber int - Line number for error reporting
 * @return Generic* (TYPE_REF) - Mutable reference
 */
Generic *franz_llvm_create_ref(Generic *value, int lineNumber) {
  if (!value) {
    fprintf(stderr, "Runtime Error @ Line %d: ref requires a non-null value\n", lineNumber);
    exit(1);
  }

  // Create copy of value (ref owns the value)
  Generic *value_copy = Generic_copy(value);
  value_copy->refCount++;  // Increment because Ref will hold it

  // Create new Ref containing the value
  Ref *ref = Ref_new(value_copy);

  // Return Generic* with TYPE_REF
  return Generic_new(TYPE_REF, ref, 0);
}

/**
 * franz_llvm_deref(Generic* ref, int lineNumber) -> Generic* (stored value)
 *
 * Reads the current value stored in a mutable reference.
 * Returns a copy of the value for safety.
 *
 * @param ref Generic* - Mutable reference (TYPE_REF)
 * @param lineNumber int - Line number for error reporting
 * @return Generic* - Copy of stored value
 */
Generic *franz_llvm_deref(Generic *ref, int lineNumber) {
  if (!ref) {
    fprintf(stderr, "Runtime Error @ Line %d: deref requires a non-null reference\n", lineNumber);
    exit(1);
  }

  if (ref->type != TYPE_REF) {
    fprintf(stderr, "Runtime Error @ Line %d: deref requires a reference, got %s\n",
            lineNumber, getTypeString(ref->type));
    exit(1);
  }

  // Extract Ref* from Generic*
  Ref *ref_ptr = (Ref *)ref->p_val;

  // Get current value (returns copy for safety)
  return Ref_get(ref_ptr);
}

/**
 * franz_llvm_set_ref(Generic* ref, Generic* value, int lineNumber) -> Generic* (TYPE_VOID)
 *
 * Updates a mutable reference with a new value.
 * Side effect: Mutates the reference.
 *
 * @param ref Generic* - Mutable reference (TYPE_REF)
 * @param value Generic* - New value to store
 * @param lineNumber int - Line number for error reporting
 * @return Generic* (TYPE_VOID)
 */
Generic *franz_llvm_set_ref(Generic *ref, Generic *value, int lineNumber) {
  if (!ref) {
    fprintf(stderr, "Runtime Error @ Line %d: set! requires a non-null reference\n", lineNumber);
    exit(1);
  }

  if (ref->type != TYPE_REF) {
    fprintf(stderr, "Runtime Error @ Line %d: set! requires a reference, got %s\n",
            lineNumber, getTypeString(ref->type));
    exit(1);
  }

  if (!value) {
    fprintf(stderr, "Runtime Error @ Line %d: set! requires a non-null value\n", lineNumber);
    exit(1);
  }

  // Extract Ref* from Generic*
  Ref *ref_ptr = (Ref *)ref->p_val;

  // Copy the new value (ref owns the value)
  Generic *new_value = Generic_copy(value);
  new_value->refCount++;  // Increment because Ref will hold it

  // Update the reference
  Ref_set(ref_ptr, new_value);

  // Return TYPE_VOID
  return Generic_new(TYPE_VOID, NULL, 0);
}

// ============================================================================
//  Get Function (String/List Indexing and Slicing)
// ============================================================================

// franz_get(collection, start, end) -> Generic* or char*
// Handles both string slicing and list indexing/slicing
// - String single char: (get "hello" 0) -> "h" (returns char*)
// - String substring: (get "hello" 0 3) -> "hel" (returns char*)
// - List single element: (get [1,2,3] 0) -> 1 (returns Generic*)
// - List slice: (get [1,2,3] 0 2) -> [1,2] (returns Generic* wrapping List*)
void *franz_get(Generic *collection, int64_t start, int64_t end_or_unused, int has_end) {
  if (!collection) {
    fprintf(stderr, "Runtime Error: get requires non-null collection\n");
    exit(1);
  }

  if (collection->type == TYPE_STRING) {
    // String operations
    char *str = *((char **)collection->p_val);
    int len = strlen(str);

    if (start < 0 || start >= len) {
      fprintf(stderr, "Runtime Error: string index %lld out of bounds (length %d)\n",
              (long long)start, len);
      exit(1);
    }

    if (!has_end) {
      // Single character: return char*
      char *result = malloc(2);
      result[0] = str[start];
      result[1] = '\0';
      return result;
    } else {
      // Substring: return char*
      int64_t end = end_or_unused;
      if (end < start || end > len) {
        fprintf(stderr, "Runtime Error: string end index %lld out of bounds (start=%lld, length=%d)\n",
                (long long)end, (long long)start, len);
        exit(1);
      }

      int sub_len = end - start;
      char *result = malloc(sub_len + 1);
      strncpy(result, &str[start], sub_len);
      result[sub_len] = '\0';
      return result;
    }
  } else if (collection->type == TYPE_LIST) {
    // List operations
    List *list = (List *)collection->p_val;

    if (start < 0 || start >= list->len) {
      fprintf(stderr, "Runtime Error: list index %lld out of bounds (length %d)\n",
              (long long)start, list->len);
      exit(1);
    }

    if (!has_end) {
      // Single element: return Generic*
      return list->vals[start];
    } else {
      // Slice: return Generic* wrapping new List*
      int64_t end = end_or_unused;
      if (end < start || end > list->len) {
        fprintf(stderr, "Runtime Error: list end index %lld out of bounds (start=%lld, length=%d)\n",
                (long long)end, (long long)start, list->len);
        exit(1);
      }

      int slice_len = end - start;
      Generic **slice_vals = malloc(sizeof(Generic*) * slice_len);
      for (int i = 0; i < slice_len; i++) {
        slice_vals[i] = list->vals[start + i];
      }

      List *slice_list = List_new(slice_vals, slice_len);
      return Generic_new(TYPE_LIST, slice_list, 0);
    }
  } else {
    fprintf(stderr, "Runtime Error: get requires string or list, got %s\n",
            getTypeString(collection->type));
    exit(1);
  }
}

// ============================================================================
//  Advanced File Operations - List Files Helper
// ============================================================================

// Helper: List files in directory (returns Generic* wrapping List*)
Generic *franz_list_files(char *dirpath, int lineNumber) {
  List *fileList = listFiles(dirpath, lineNumber);
  return Generic_new(TYPE_LIST, fileList, 0);
}

// Helper: Print Generic* value (for println support)
void franz_print_generic(Generic *value) {
  if (!value) {
    printf("(null)");
    return;
  }

  switch (value->type) {
    case TYPE_INT: {
      long long val = (long long)(*((int *)value->p_val));
      printf("%lld", val);
      break;
    }
    case TYPE_FLOAT: {
      double *p_float = (double *)value->p_val;
      printf("%f", *p_float);
      break;
    }
    case TYPE_STRING: {
      char **p_str = (char **)value->p_val;
      printf("%s", *p_str);
      break;
    }
    case TYPE_LIST: {
      List *l = (List *)value->p_val;
      printf("[");
      for (int i = 0; i < l->len; i++) {
        franz_print_generic(l->vals[i]);
        if (i < l->len - 1) printf(", ");
      }
      printf("]");
      break;
    }
    case TYPE_DICT: {
      Dict *dict = (Dict *)value->p_val;
      Dict_print(dict);
      break;
    }
    case TYPE_FUNCTION:
    case TYPE_NATIVEFUNCTION:
      printf("<function>");
      break;
    case TYPE_BYTECODE_CLOSURE:
      printf("<closure>");
      break;
    case TYPE_VOID:
      printf("void");
      break;
    default:
      printf("<unknown type %d>", value->type);
      break;
  }
}

// Helper: Print value from LLVM (detects if Generic* or native value)
// For use by println in LLVM mode
void franz_println_value(void *value_ptr, int is_generic) {
  if (is_generic) {
    // It's a Generic* - use franz_print_generic
    franz_print_generic((Generic *)value_ptr);
  } else {
    // It's a native value (string, etc.) - print directly
    printf("%s", (char *)value_ptr);
  }
}

// ============================================================================
//  Auto-Unboxing Runtime Helpers
// ============================================================================

// Helper: Unbox Generic* to int64_t
// Returns the native int value, or 0 if not an int
int64_t franz_unbox_int(Generic *generic) {
  if (!generic) {
    fprintf(stderr, "Runtime Error: Cannot unbox NULL pointer\n");
    exit(1);
  }

  if (generic->type == TYPE_INT) {
    return (int64_t)(*((int *)generic->p_val));
  } else if (generic->type == TYPE_FLOAT) {
    // Auto-convert float to int
    return (int64_t)(*((double *)generic->p_val));
  } else {
    fprintf(stderr, "Runtime Error: Cannot unbox %s to int\n",
            getTypeString(generic->type));
    exit(1);
  }
}

// Helper: Unbox Generic* to double
// Returns the native float value, or 0.0 if not a float
double franz_unbox_float(Generic *generic) {
  if (!generic) {
    fprintf(stderr, "Runtime Error: Cannot unbox NULL pointer\n");
    exit(1);
  }

  if (generic->type == TYPE_FLOAT) {
    return *((double *)generic->p_val);
  } else if (generic->type == TYPE_INT) {
    // Auto-convert int to float
    return (double)(*((int *)generic->p_val));
  } else {
    fprintf(stderr, "Runtime Error: Cannot unbox %s to float\n",
            getTypeString(generic->type));
    exit(1);
  }
}

// Helper: Unbox Generic* to string
// Returns the native string pointer, or NULL if not a string
char *franz_unbox_string(Generic *generic) {
  if (!generic) {
    fprintf(stderr, "Runtime Error: Cannot unbox NULL pointer\n");
    exit(1);
  }

  if (generic->type == TYPE_STRING) {
    return *((char **)generic->p_val);
  } else {
    fprintf(stderr, "Runtime Error: Cannot unbox %s to string\n",
            getTypeString(generic->type));
    exit(1);
  }
}

// ===========================================================================
// Dict Runtime Wrappers for LLVM
// ===========================================================================

// franz_dict_new(capacity) -> Dict*
void *franz_dict_new(int capacity) {
  return Dict_new(capacity);
}

// franz_dict_set_inplace(dict, key, value) -> void
void franz_dict_set_inplace(void *dict, void *key, void *value) {
  Dict_set_inplace((Dict *)dict, (Generic *)key, (Generic *)value);
}

// franz_dict_get(dict, key) -> Generic*
void *franz_dict_get(void *dict, void *key) {
  return Dict_get((Dict *)dict, (Generic *)key);
}

// franz_dict_set(dict, key, value) -> Generic* (boxed dict)
void *franz_dict_set(void *dict, void *key, void *value) {
  Dict *new_dict = Dict_set((Dict *)dict, (Generic *)key, (Generic *)value);
  return Generic_new(TYPE_DICT, new_dict, 0);
}

// franz_dict_has(dict, key) -> int
int franz_dict_has(void *dict, void *key) {
  return Dict_has((Dict *)dict, (Generic *)key);
}

// franz_dict_keys(dict, count_out) -> Generic**
void *franz_dict_keys(void *dict, int *count_out) {
  return Dict_keys((Dict *)dict, count_out);
}

// franz_dict_values(dict, count_out) -> Generic**
void *franz_dict_values(void *dict, int *count_out) {
  return Dict_values((Dict *)dict, count_out);
}

// franz_dict_merge(dict1, dict2) -> Generic* (boxed dict)
void *franz_dict_merge(void *dict1, void *dict2) {
  Dict *merged = Dict_merge((Dict *)dict1, (Dict *)dict2);
  return Generic_new(TYPE_DICT, merged, 0);
}

// franz_box_dict(dict) -> Generic*
void *franz_box_dict(void *dict) {
  return Generic_new(TYPE_DICT, dict, 0);
}

// franz_unbox_dict(generic) -> Dict*
void *franz_unbox_dict(void *generic) {
  Generic *g = (Generic *)generic;
  if (g->type != TYPE_DICT) {
    fprintf(stderr, "Runtime Error: Cannot unbox %s to dict\n",
            getTypeString(g->type));
    exit(1);
  }
  return (Dict *)g->p_val;
}

// franz_dict_map(dict_generic, closure_generic, lineNumber) -> Generic* (new dict)
// Runtime fallback for dict_map - calls closure for each key-value pair
void *franz_dict_map(void *dict_generic, void *closure_generic, int lineNumber) {
  fprintf(stderr, "[RUNTIME DEBUG] franz_dict_map called at line %d\n", lineNumber);
  fprintf(stderr, "[RUNTIME DEBUG] dict_generic=%p, closure_generic=%p\n", dict_generic, closure_generic);

  Generic *dict_gen = (Generic *)dict_generic;
  Generic *closure_gen = (Generic *)closure_generic;

  fprintf(stderr, "[RUNTIME DEBUG] dict_gen type=%d, closure_gen type=%d\n", dict_gen->type, closure_gen->type);

  if (dict_gen->type != TYPE_DICT) {
    fprintf(stderr, "Runtime Error @ Line %d: dict_map expects dict as first argument\n", lineNumber);
    exit(1);
  }

  fprintf(stderr, "[RUNTIME DEBUG] Dict validation passed\n");

  Dict *dict = (Dict *)dict_gen->p_val;
  Dict *new_dict = Dict_new(dict->capacity);

  fprintf(stderr, "[RUNTIME DEBUG] Starting iteration, capacity=%d\n", dict->capacity);

  // Iterate through all entries
  int entry_count = 0;
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      entry_count++;
      fprintf(stderr, "[RUNTIME DEBUG] Processing entry %d at bucket %d\n", entry_count, i);

      Generic *key = entry->key;
      Generic *value = entry->value;

      fprintf(stderr, "[RUNTIME DEBUG] key type=%d, value type=%d\n", key->type, value->type);

      // Call closure with key and value
      key->refCount++;
      value->refCount++;

      Generic *fn_args[] = {key, value};

      fprintf(stderr, "[RUNTIME DEBUG] Calling closure with 2 args\n");
      closure_gen->refCount++;

      // Check if LLVM closure or runtime closure
      Generic *new_value;
      if (closure_gen->type == TYPE_BYTECODE_CLOSURE) {
        // LLVM closure - use special caller
        fprintf(stderr, "[RUNTIME DEBUG] Detected LLVM closure, using franz_call_llvm_closure\n");
        new_value = franz_call_llvm_closure(closure_gen, fn_args, 2, lineNumber);
      } else {
        // Runtime closure - use applyFunc
        fprintf(stderr, "[RUNTIME DEBUG] Detected runtime closure, using applyFunc\n");
        new_value = applyFunc(closure_gen, NULL, fn_args, 2, lineNumber);
      }

      closure_gen->refCount--;

      fprintf(stderr, "[RUNTIME DEBUG] Closure returned, new_value type=%d\n", new_value->type);

      key->refCount--;
      value->refCount--;

      // Insert into new dict
      Dict_set_inplace(new_dict, key, new_value);

      if (new_value->refCount == 0) Generic_free(new_value);

      entry = entry->next;
    }
  }

  fprintf(stderr, "[RUNTIME DEBUG] Iteration complete, processed %d entries\n", entry_count);
  fprintf(stderr, "[RUNTIME DEBUG] Returning new dict\n");

  return Generic_new(TYPE_DICT, new_dict, 0);
}

// franz_dict_filter(dict_generic, closure_generic, lineNumber) -> Generic* (filtered dict)
// Runtime fallback for dict_filter - keeps entries where closure returns truthy
void *franz_dict_filter(void *dict_generic, void *closure_generic, int lineNumber) {
  fprintf(stderr, "[RUNTIME DEBUG] franz_dict_filter called at line %d\n", lineNumber);
  fprintf(stderr, "[RUNTIME DEBUG] dict_generic=%p, closure_generic=%p\n", dict_generic, closure_generic);

  Generic *dict_gen = (Generic *)dict_generic;
  Generic *closure_gen = (Generic *)closure_generic;

  fprintf(stderr, "[RUNTIME DEBUG] dict_gen type=%d, closure_gen type=%d\n", dict_gen->type, closure_gen->type);

  if (dict_gen->type != TYPE_DICT) {
    fprintf(stderr, "Runtime Error @ Line %d: dict_filter expects dict as first argument\n", lineNumber);
    exit(1);
  }

  fprintf(stderr, "[RUNTIME DEBUG] Dict validation passed\n");

  Dict *dict = (Dict *)dict_gen->p_val;
  Dict *new_dict = Dict_new(dict->capacity);

  fprintf(stderr, "[RUNTIME DEBUG] Starting iteration, capacity=%d\n", dict->capacity);

  // Iterate through all entries
  int entry_count = 0;
  int kept_count = 0;
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      entry_count++;
      fprintf(stderr, "[RUNTIME DEBUG] Processing entry %d at bucket %d\n", entry_count, i);

      Generic *key = entry->key;
      Generic *value = entry->value;

      fprintf(stderr, "[RUNTIME DEBUG] key type=%d, value type=%d\n", key->type, value->type);

      // Call closure with key and value
      key->refCount++;
      value->refCount++;

      Generic *fn_args[] = {key, value};

      fprintf(stderr, "[RUNTIME DEBUG] Calling closure with 2 args\n");
      closure_gen->refCount++;

      // Check if LLVM closure or runtime closure
      Generic *result;
      if (closure_gen->type == TYPE_BYTECODE_CLOSURE) {
        // LLVM closure - use special caller
        fprintf(stderr, "[RUNTIME DEBUG] Detected LLVM closure, using franz_call_llvm_closure\n");
        result = franz_call_llvm_closure(closure_gen, fn_args, 2, lineNumber);
      } else {
        // Runtime closure - use applyFunc
        fprintf(stderr, "[RUNTIME DEBUG] Detected runtime closure, using applyFunc\n");
        result = applyFunc(closure_gen, NULL, fn_args, 2, lineNumber);
      }

      closure_gen->refCount--;

      fprintf(stderr, "[RUNTIME DEBUG] Closure returned, result type=%d\n", result->type);

      key->refCount--;
      value->refCount--;

      // Check if result is truthy (non-zero for int, non-void)
      int keep = 0;
      if (result->type == TYPE_INT) {
        keep = (*((int *)result->p_val) != 0);
        fprintf(stderr, "[RUNTIME DEBUG] Result is INT, value=%d, keep=%d\n", *((int *)result->p_val), keep);
      } else if (result->type != TYPE_VOID) {
        keep = 1;  // Non-void is truthy
        fprintf(stderr, "[RUNTIME DEBUG] Result is non-void, keep=1\n");
      } else {
        fprintf(stderr, "[RUNTIME DEBUG] Result is void, keep=0\n");
      }

      if (keep) {
        Dict_set_inplace(new_dict, key, value);
        kept_count++;
        fprintf(stderr, "[RUNTIME DEBUG] Entry kept, total kept=%d\n", kept_count);
      } else {
        fprintf(stderr, "[RUNTIME DEBUG] Entry filtered out\n");
      }

      if (result->refCount == 0) Generic_free(result);

      entry = entry->next;
    }
  }

  fprintf(stderr, "[RUNTIME DEBUG] Iteration complete, processed %d entries, kept %d\n", entry_count, kept_count);
  fprintf(stderr, "[RUNTIME DEBUG] Returning filtered dict\n");

  return Generic_new(TYPE_DICT, new_dict, 0);
}

/**
 *  Generic equality comparison for LLVM mode
 *
 * Compares two Generic* values for equality, handling all types properly.
 * Used when LLVM compiler encounters mixed types or Generic* values in `is` comparison.
 *
 * This is a simple wrapper around Generic_is() for LLVM native code.
 *
 * @param a First Generic* value
 * @param b Second Generic* value
 * @return 1 if equal, 0 if not equal
 */
int franz_generic_is(void *a, void *b) {
  if (!a || !b) return 0;

  fprintf(stderr, "[GENERIC IS DEBUG] Comparing Generic* %p and %p\n", a, b);
  Generic *ga = (Generic *)a;
  Generic *gb = (Generic *)b;
  fprintf(stderr, "[GENERIC IS DEBUG] Types: %d and %d\n", ga->type, gb->type);

  if (ga->type == TYPE_STRING && gb->type == TYPE_STRING) {
    char *sa = *((char **)ga->p_val);
    char *sb = *((char **)gb->p_val);
    fprintf(stderr, "[GENERIC IS DEBUG] String comparison: '%s' vs '%s'\n", sa, sb);
  }

  int result = Generic_is(ga, gb);
  fprintf(stderr, "[GENERIC IS DEBUG] Result: %d\n", result);
  return result;
}

/**
 *  Runtime type introspection for LLVM mode
 *
 * Returns the type name of a Generic* value as a string.
 * Used by the type() function in LLVM native compilation.
 *
 * @param generic_ptr Generic* pointer to examine
 * @return const char* - Type name string (static, no need to free)
 *
 * Returned strings:
 * - "integer" for TYPE_INT
 * - "float" for TYPE_FLOAT
 * - "string" for TYPE_STRING
 * - "list" for TYPE_LIST
 * - "function" for TYPE_FUNCTION
 * - "closure" for TYPE_BYTECODE_CLOSURE
 * - "void" for TYPE_VOID
 * - "dict" for TYPE_DICT
 * - "native function" for TYPE_NATIVEFUNCTION
 * - "unknown" for unrecognized types
 */
const char* franz_get_type_string(void *generic_ptr) {
  if (!generic_ptr) {
    fprintf(stderr, "[TYPE DEBUG] NULL pointer passed to franz_get_type_string\n");
    return "void";
  }

  Generic *g = (Generic *)generic_ptr;
  fprintf(stderr, "[TYPE DEBUG] Examining Generic* at %p, type=%d\n", generic_ptr, g->type);

  // Use getTypeString helper (defined in generic.c)
  const char *typeStr = getTypeString(g->type);
  fprintf(stderr, "[TYPE DEBUG] Type string: '%s'\n", typeStr);

  return typeStr;
}
