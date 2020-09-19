#include "llvm_codegen.h"
#include "../string.h"  // For parseString() - escape sequence processing
#include "../number-formats/number_parse.h"  //  Multi-base number parsing
#include "../type-inference/type_infer.h"  //  Type inference for float/string functions
#include "../llvm-math/llvm_math.h"  //  Enhanced math functions
#include "../llvm-comparisons/llvm_comparisons.h"  //  Comparison operators
#include "../llvm-logical/llvm_logical.h"  //  Logical operators with short-circuit evaluation
#include "../llvm-control-flow/llvm_control_flow.h"  //  Control flow (if, when, unless)
#include "../llvm-type-guards/llvm_type_guards.h"  //  Type guards (is_int, is_float, is_string)
#include "../llvm-terminal/llvm_terminal.h"  //  Terminal dimension functions (rows, columns)
#include "../llvm-closures/llvm_closures.h"  //  Closure support (lexical scope capture)
#include "../llvm-lists/llvm_lists.h"  //  List literal support
#include "../llvm-list-ops/llvm_list_ops.h"  //  List operations (head, tail, cons, etc.)
#include "../llvm-filter/llvm_filter.h"  // LLVM Filter implementation
#include "../llvm-map/llvm_map.h"  // LLVM Map implementation
#include "../llvm-reduce/llvm_reduce.h"  // LLVM Reduce implementation
#include "../llvm-unboxing/llvm_unboxing.h"  //  Auto-unboxing for list operations
#include "../llvm-file-ops/llvm_file_ops.h"  //  File operations (read_file, write_file)
#include "../llvm-file-advanced/llvm_file_advanced.h"  //  Advanced file operations (binary, dir, metadata)
#include "../llvm-modules/llvm_modules.h"  //  Module system (use, use_as, use_with)
#include "../llvm-adt/llvm_adt.h"  //  ADT support (variant, match)
#include "../llvm-dict/llvm_dict.h"  // Dict support (hash maps)
#include "../llvm-string-ops/llvm_string_ops.h"  //  String operations (get substring)
#include "../llvm-type/llvm_type.h"  //  Type introspection (type function)
#include "../llvm-refs/llvm_refs.h"  //  Mutable references (ref, deref, set!)
#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//  LLVM Code Generator Implementation
// Basic LLVM IR generation for arithmetic, variables, and output
//  Type inference for float/string user-defined functions

// ============================================================================
// Variable Map Implementation (Simple Hash Map)
// ============================================================================

LLVMVariableMap *LLVMVariableMap_new() {
  LLVMVariableMap *map = (LLVMVariableMap *)malloc(sizeof(LLVMVariableMap));
  map->capacity = 16;
  map->count = 0;
  map->entries = (LLVMVariable *)calloc(map->capacity, sizeof(LLVMVariable));
  return map;
}

void LLVMVariableMap_free(LLVMVariableMap *map) {
  if (!map) return;
  for (int i = 0; i < map->count; i++) {
    free(map->entries[i].name);
  }
  free(map->entries);
  free(map);
}

void LLVMVariableMap_set(LLVMVariableMap *map, const char *name, LLVMValueRef value) {
  // Check if variable already exists (update)
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->entries[i].name, name) == 0) {
      map->entries[i].value = value;
      return;
    }
  }

  // Grow if needed
  if (map->count >= map->capacity) {
    map->capacity *= 2;
    map->entries = (LLVMVariable *)realloc(map->entries,
                                           map->capacity * sizeof(LLVMVariable));
  }

  // Add new variable
  map->entries[map->count].name = strdup(name);
  map->entries[map->count].value = value;
  map->count++;
}

LLVMValueRef LLVMVariableMap_get(LLVMVariableMap *map, const char *name) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->entries[i].name, name) == 0) {
      return map->entries[i].value;
    }
  }
  return NULL;
}

// ============================================================================
//  Type Inference Bridge Functions
// ============================================================================

/**
 * Converts InferredTypeKind (from type inference) to ClosureReturnTypeTag.
 * This bridges Franz's type inference system with LLVM return type optimization.
 *
 * @param kind Inferred type kind from TypeInfer_inferFunction
 * @return ClosureReturnTypeTag (INT=0, FLOAT=1, POINTER=2, CLOSURE=3, VOID=4, DYNAMIC=5)
 */
static int TypeInfer_toReturnTypeTag(InferredTypeKind kind) {
  switch (kind) {
    case INFER_TYPE_INT:
      return CLOSURE_RETURN_INT;      // Integer returns (0) - direct i64
    case INFER_TYPE_FLOAT:
      return CLOSURE_RETURN_FLOAT;    // Float returns (1) - direct double
    case INFER_TYPE_STRING:
      return CLOSURE_RETURN_POINTER;  // String returns (2) - i8* pointers
    case INFER_TYPE_VOID:
      return CLOSURE_RETURN_VOID;     // Void returns (4) - canonical void
    case INFER_TYPE_UNKNOWN:
      return CLOSURE_RETURN_DYNAMIC;  // Polymorphic/unknown (5) - runtime check
    default:
      return CLOSURE_RETURN_POINTER;  // Conservative default (2) - treat as pointer
  }
}

// ============================================================================
// LLVM Code Generator Initialization
// ============================================================================

LLVMCodeGen *LLVMCodeGen_new_impl(const char *moduleName) {
  LLVMCodeGen *gen = (LLVMCodeGen *)malloc(sizeof(LLVMCodeGen));
  if (!gen) {
    fprintf(stderr, "Failed to allocate LLVMCodeGen\n");
    return NULL;
  }

  // Initialize LLVM context and module
  gen->context = LLVMContextCreate();
  gen->module = LLVMModuleCreateWithNameInContext(moduleName, gen->context);
  gen->builder = LLVMCreateBuilderInContext(gen->context);

  // Initialize type cache
  gen->intType = LLVMInt64TypeInContext(gen->context);
  gen->floatType = LLVMDoubleTypeInContext(gen->context);
  gen->stringType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  gen->voidType = LLVMVoidTypeInContext(gen->context);
  gen->genericType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  gen->currentFunction = NULL;
  gen->currentScope = NULL;
  gen->debugMode = 0;

  // TCO: Initialize tail call optimization flags (enabled by default - functional language standard)
  gen->enableTCO = 1;        // Enabled by default (matching OCaml/Scheme), use --no-tco to disable
  gen->inTailPosition = 0;   // Not in tail position initially
  gen->currentClosureReturnTag = -1;

  //  Initialize loop context (NULL = not in loop)
  gen->loopExitBlock = NULL;
  gen->loopIncrBlock = NULL;
  gen->loopReturnPtr = NULL;
  gen->loopReturnType = NULL;

  //  Initialize recursion support (NULL = not compiling function)
  gen->currentFunctionName = NULL;

  // Initialize variable map
  gen->variables = LLVMVariableMap_new();

  //  Initialize function map for user-defined functions
  gen->functions = LLVMVariableMap_new();

  //  Initialize closure tracking map (LLVM 17 opaque pointer compatibility)
  gen->closures = LLVMVariableMap_new();

  //  Initialize global symbol registry (Rust-style)
  // Will be populated with all built-in functions below
  gen->globalSymbols = LLVMVariableMap_new();

  //  Initialize generic pointer tracking
  // Tracks which variables hold Generic* (lists) vs native values (strings)
  gen->genericVariables = LLVMVariableMap_new();

  //  Initialize void tracking for equality support
  gen->voidVariables = LLVMVariableMap_new();

  //  Initialize type metadata tracking for type() function
  // Tracks the AST opcode (type) of each variable for runtime type introspection
  gen->typeMetadata = LLVMVariableMap_new();
  gen->paramTypeTags = NULL;  // Set per-closure during compilation

  //  Initialize return type tracking for upstream tagging optimization
  // Maps function name → return type tag (TYPE_INT, TYPE_FLOAT, TYPE_CLOSURE, etc.)
  gen->returnTypeTags = LLVMVariableMap_new();

  // Declare printf for output
  LLVMTypeRef printfParams[] = {gen->stringType};
  gen->printfType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context),
                                     printfParams, 1, 1); // varargs
  gen->printfFunc = LLVMAddFunction(gen->module, "printf", gen->printfType);

  // Declare puts for println
  LLVMTypeRef putsParams[] = {gen->stringType};
  gen->putsType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context),
                                   putsParams, 1, 0);
  gen->putsFunc = LLVMAddFunction(gen->module, "puts", gen->putsType);

  // Declare fflush(NULL) to force stdout flushes (ensures println output visible when buffered)
  LLVMTypeRef fflushParams[] = {gen->stringType};
  gen->fflushType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context),
                                     fflushParams, 1, 0);
  gen->fflushFunc = LLVMAddFunction(gen->module, "fflush", gen->fflushType);

  //  Declare C runtime functions for type conversions
  // atoi: string -> int
  LLVMTypeRef atoiParams[] = {gen->stringType};
  gen->atoiType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context),
                                   atoiParams, 1, 0);
  gen->atoiFunc = LLVMAddFunction(gen->module, "atoi", gen->atoiType);

  // atof: string -> double
  LLVMTypeRef atofParams[] = {gen->stringType};
  gen->atofType = LLVMFunctionType(gen->floatType,
                                   atofParams, 1, 0);
  gen->atofFunc = LLVMAddFunction(gen->module, "atof", gen->atofType);

  // snprintf: for int/float -> string conversion
  LLVMTypeRef snprintfParams[] = {gen->stringType, LLVMInt64TypeInContext(gen->context), gen->stringType};
  gen->snprintfType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context),
                                       snprintfParams, 3, 1); // varargs
  gen->snprintfFunc = LLVMAddFunction(gen->module, "snprintf", gen->snprintfType);

  //  Formatting functions from number_parse module
  // formatInteger: (i64, i32) -> i8*
  LLVMTypeRef formatIntParams[] = {gen->intType, LLVMInt32TypeInContext(gen->context)};
  gen->formatIntegerType = LLVMFunctionType(gen->stringType, formatIntParams, 2, 0);
  gen->formatIntegerFunc = LLVMAddFunction(gen->module, "formatInteger", gen->formatIntegerType);

  // formatFloat: (double, i32) -> i8*
  LLVMTypeRef formatFloatParams[] = {gen->floatType, LLVMInt32TypeInContext(gen->context)};
  gen->formatFloatType = LLVMFunctionType(gen->stringType, formatFloatParams, 2, 0);
  gen->formatFloatFunc = LLVMAddFunction(gen->module, "formatFloat", gen->formatFloatType);

  //  String manipulation runtime functions
  // strlen: (i8*) -> i64
  LLVMTypeRef strlenParams[] = {gen->stringType};
  gen->strlenType = LLVMFunctionType(gen->intType, strlenParams, 1, 0);
  gen->strlenFunc = LLVMAddFunction(gen->module, "strlen", gen->strlenType);

  // strcpy: (i8*, i8*) -> i8*
  LLVMTypeRef strcpyParams[] = {gen->stringType, gen->stringType};
  gen->strcpyType = LLVMFunctionType(gen->stringType, strcpyParams, 2, 0);
  gen->strcpyFunc = LLVMAddFunction(gen->module, "strcpy", gen->strcpyType);

  // strcat: (i8*, i8*) -> i8*
  LLVMTypeRef strcatParams[] = {gen->stringType, gen->stringType};
  gen->strcatType = LLVMFunctionType(gen->stringType, strcatParams, 2, 0);
  gen->strcatFunc = LLVMAddFunction(gen->module, "strcat", gen->strcatType);

  //  strcmp: (i8*, i8*) -> i32
  LLVMTypeRef strcmpParams[] = {gen->stringType, gen->stringType};
  gen->strcmpType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context), strcmpParams, 2, 0);
  gen->strcmpFunc = LLVMAddFunction(gen->module, "strcmp", gen->strcmpType);

  // malloc: (i64) -> i8*
  if (!gen->mallocFunc) {  // Only declare if not already declared
    LLVMTypeRef mallocParams[] = {gen->intType};
    gen->mallocType = LLVMFunctionType(gen->stringType, mallocParams, 1, 0);
    gen->mallocFunc = LLVMAddFunction(gen->module, "malloc", gen->mallocType);
  }

  //  Math runtime functions from math.h
  // fmod: (double, double) -> double
  LLVMTypeRef fmodParams[] = {gen->floatType, gen->floatType};
  gen->fmodType = LLVMFunctionType(gen->floatType, fmodParams, 2, 0);
  gen->fmodFunc = LLVMAddFunction(gen->module, "fmod", gen->fmodType);

  // pow: (double, double) -> double
  LLVMTypeRef powParams[] = {gen->floatType, gen->floatType};
  gen->powType = LLVMFunctionType(gen->floatType, powParams, 2, 0);
  gen->powFunc = LLVMAddFunction(gen->module, "pow", gen->powType);

  // rand: () -> i32
  gen->randType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context), NULL, 0, 0);
  gen->randFunc = LLVMAddFunction(gen->module, "rand", gen->randType);

  // srand: (i32) -> void
  LLVMTypeRef srandParams[] = {LLVMInt32TypeInContext(gen->context)};
  gen->srandType = LLVMFunctionType(gen->voidType, srandParams, 1, 0);
  gen->srandFunc = LLVMAddFunction(gen->module, "srand", gen->srandType);

  // floor: (double) -> double
  LLVMTypeRef floorParams[] = {gen->floatType};
  gen->floorType = LLVMFunctionType(gen->floatType, floorParams, 1, 0);
  gen->floorFunc = LLVMAddFunction(gen->module, "floor", gen->floorType);

  // ceil: (double) -> double
  LLVMTypeRef ceilParams[] = {gen->floatType};
  gen->ceilType = LLVMFunctionType(gen->floatType, ceilParams, 1, 0);
  gen->ceilFunc = LLVMAddFunction(gen->module, "ceil", gen->ceilType);

  // round: (double) -> double
  LLVMTypeRef roundParams[] = {gen->floatType};
  gen->roundType = LLVMFunctionType(gen->floatType, roundParams, 1, 0);
  gen->roundFunc = LLVMAddFunction(gen->module, "round", gen->roundType);

  // fabs: (double) -> double
  LLVMTypeRef fabsParams[] = {gen->floatType};
  gen->fabsType = LLVMFunctionType(gen->floatType, fabsParams, 1, 0);
  gen->fabsFunc = LLVMAddFunction(gen->module, "fabs", gen->fabsType);

  // sqrt: (double) -> double
  LLVMTypeRef sqrtParams[] = {gen->floatType};
  gen->sqrtType = LLVMFunctionType(gen->floatType, sqrtParams, 1, 0);
  gen->sqrtFunc = LLVMAddFunction(gen->module, "sqrt", gen->sqrtType);

  //  I/O function declarations
  // getchar: () -> i32
  gen->getcharType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context), NULL, 0, 0);
  gen->getcharFunc = LLVMAddFunction(gen->module, "getchar", gen->getcharType);

  // realloc: (i8*, i64) -> i8*
  LLVMTypeRef reallocParams[] = {gen->stringType, gen->intType};
  gen->reallocType = LLVMFunctionType(gen->stringType, reallocParams, 2, 0);
  gen->reallocFunc = LLVMAddFunction(gen->module, "realloc", gen->reallocType);

  //  Terminal dimension function declarations
  // franz_get_terminal_rows: () -> i64
  gen->getTerminalRowsType = LLVMFunctionType(gen->intType, NULL, 0, 0);
  gen->getTerminalRowsFunc = LLVMAddFunction(gen->module, "franz_get_terminal_rows", gen->getTerminalRowsType);

  // franz_get_terminal_columns: () -> i64
  gen->getTerminalColumnsType = LLVMFunctionType(gen->intType, NULL, 0, 0);
  gen->getTerminalColumnsFunc = LLVMAddFunction(gen->module, "franz_get_terminal_columns", gen->getTerminalColumnsType);

  // franz_repeat_string: (ptr, i64) -> ptr
  LLVMTypeRef repeatStringParams[] = {gen->stringType, gen->intType};
  gen->repeatStringType = LLVMFunctionType(gen->stringType, repeatStringParams, 2, 0);
  gen->repeatStringFunc = LLVMAddFunction(gen->module, "franz_repeat_string", gen->repeatStringType);

  //  Register all built-in functions as global symbols (Rust-style)
  // These symbols have external/internal linkage and are NOT captured in closures
  // Dummy value (gen->printfFunc) - we only check if key exists
  LLVMValueRef marker = gen->printfFunc;  // Any non-NULL value works as marker

  // Arithmetic operations
  LLVMVariableMap_set(gen->globalSymbols, "add", marker);
  LLVMVariableMap_set(gen->globalSymbols, "subtract", marker);
  LLVMVariableMap_set(gen->globalSymbols, "multiply", marker);
  LLVMVariableMap_set(gen->globalSymbols, "divide", marker);
  LLVMVariableMap_set(gen->globalSymbols, "remainder", marker);
  LLVMVariableMap_set(gen->globalSymbols, "power", marker);

  // Math functions
  LLVMVariableMap_set(gen->globalSymbols, "random", marker);
  LLVMVariableMap_set(gen->globalSymbols, "random_int", marker);
  LLVMVariableMap_set(gen->globalSymbols, "random_range", marker);
  LLVMVariableMap_set(gen->globalSymbols, "random_seed", marker);
  LLVMVariableMap_set(gen->globalSymbols, "floor", marker);
  LLVMVariableMap_set(gen->globalSymbols, "ceil", marker);
  LLVMVariableMap_set(gen->globalSymbols, "round", marker);
  LLVMVariableMap_set(gen->globalSymbols, "abs", marker);
  LLVMVariableMap_set(gen->globalSymbols, "min", marker);
  LLVMVariableMap_set(gen->globalSymbols, "max", marker);
  LLVMVariableMap_set(gen->globalSymbols, "sqrt", marker);

  // Comparison operators
  LLVMVariableMap_set(gen->globalSymbols, "is", marker);
  LLVMVariableMap_set(gen->globalSymbols, "less_than", marker);
  LLVMVariableMap_set(gen->globalSymbols, "greater_than", marker);

  // Logical operators
  LLVMVariableMap_set(gen->globalSymbols, "not", marker);
  LLVMVariableMap_set(gen->globalSymbols, "and", marker);
  LLVMVariableMap_set(gen->globalSymbols, "or", marker);

  // Control flow
  LLVMVariableMap_set(gen->globalSymbols, "if", marker);
  LLVMVariableMap_set(gen->globalSymbols, "when", marker);
  LLVMVariableMap_set(gen->globalSymbols, "unless", marker);
  LLVMVariableMap_set(gen->globalSymbols, "cond", marker);
  LLVMVariableMap_set(gen->globalSymbols, "loop", marker);
  LLVMVariableMap_set(gen->globalSymbols, "while", marker);
  LLVMVariableMap_set(gen->globalSymbols, "break", marker);
  LLVMVariableMap_set(gen->globalSymbols, "continue", marker);

  // Type guards
  LLVMVariableMap_set(gen->globalSymbols, "is_int", marker);
  LLVMVariableMap_set(gen->globalSymbols, "is_float", marker);
  LLVMVariableMap_set(gen->globalSymbols, "is_string", marker);
  LLVMVariableMap_set(gen->globalSymbols, "is_list", marker);
  LLVMVariableMap_set(gen->globalSymbols, "is_function", marker);

  // I/O functions
  LLVMVariableMap_set(gen->globalSymbols, "println", marker);
  LLVMVariableMap_set(gen->globalSymbols, "print", marker);
  LLVMVariableMap_set(gen->globalSymbols, "input", marker);

  // Terminal functions
  LLVMVariableMap_set(gen->globalSymbols, "rows", marker);
  LLVMVariableMap_set(gen->globalSymbols, "columns", marker);
  LLVMVariableMap_set(gen->globalSymbols, "repeat", marker);

  // File operations
  LLVMVariableMap_set(gen->globalSymbols, "read_file", marker);
  LLVMVariableMap_set(gen->globalSymbols, "write_file", marker);
  LLVMVariableMap_set(gen->globalSymbols, "file_exists", marker);
  LLVMVariableMap_set(gen->globalSymbols, "append_file", marker);

  //  Advanced file operations
  LLVMVariableMap_set(gen->globalSymbols, "read_binary", marker);
  LLVMVariableMap_set(gen->globalSymbols, "write_binary", marker);
  LLVMVariableMap_set(gen->globalSymbols, "list_files", marker);
  LLVMVariableMap_set(gen->globalSymbols, "create_dir", marker);
  LLVMVariableMap_set(gen->globalSymbols, "dir_exists", marker);
  LLVMVariableMap_set(gen->globalSymbols, "remove_dir", marker);
  LLVMVariableMap_set(gen->globalSymbols, "file_size", marker);
  LLVMVariableMap_set(gen->globalSymbols, "file_mtime", marker);
  LLVMVariableMap_set(gen->globalSymbols, "is_directory", marker);

  // Type conversion functions
  LLVMVariableMap_set(gen->globalSymbols, "integer", marker);
  LLVMVariableMap_set(gen->globalSymbols, "float", marker);
  LLVMVariableMap_set(gen->globalSymbols, "string", marker);

  // String operations
  LLVMVariableMap_set(gen->globalSymbols, "format-int", marker);
  LLVMVariableMap_set(gen->globalSymbols, "format-float", marker);
  LLVMVariableMap_set(gen->globalSymbols, "join", marker);

  //  Mutable references
  LLVMVariableMap_set(gen->globalSymbols, "ref", marker);
  LLVMVariableMap_set(gen->globalSymbols, "deref", marker);
  LLVMVariableMap_set(gen->globalSymbols, "set!", marker);

  return gen;
}

void LLVMCodeGen_free_impl(LLVMCodeGen *gen) {
  if (!gen) return;

  if (gen->variables) LLVMVariableMap_free(gen->variables);
  if (gen->functions) LLVMVariableMap_free(gen->functions);      //  Free function map
  if (gen->closures) LLVMVariableMap_free(gen->closures);        //  Free closure tracking map
  if (gen->globalSymbols) LLVMVariableMap_free(gen->globalSymbols);  //  Free global symbol registry
  if (gen->genericVariables) LLVMVariableMap_free(gen->genericVariables);  //  Free generic pointer tracking
  if (gen->voidVariables) LLVMVariableMap_free(gen->voidVariables);  //  Free void tracking map
  if (gen->paramTypeTags) LLVMVariableMap_free(gen->paramTypeTags);  //  Free param tag tracking
  if (gen->typeMetadata) LLVMVariableMap_free(gen->typeMetadata);    //  Free type metadata tracking
  if (gen->returnTypeTags) LLVMVariableMap_free(gen->returnTypeTags);  //  Free return type tracking
  if (gen->builder) LLVMDisposeBuilder(gen->builder);
  if (gen->module) LLVMDisposeModule(gen->module);
  if (gen->context) LLVMContextDispose(gen->context);

  free(gen);
}

// ============================================================================
//  Node Compilation Functions
// ============================================================================

// Forward declaration
LLVMValueRef LLVMCodeGen_compileNode_impl(LLVMCodeGen *gen, AstNode *node);

// Compile integer literal
LLVMValueRef LLVMCodeGen_compileInteger_impl(LLVMCodeGen *gen, AstNode *node) {
  if (!node || !node->val) {
    fprintf(stderr, "ERROR: Invalid integer node\n");
    return NULL;
  }

  //  Use multi-base parser (supports 0x, 0b, 0o)
  int64_t value = parseInteger(node->val);
  return LLVMConstInt(gen->intType, value, 0);
}

// Compile float literal
LLVMValueRef LLVMCodeGen_compileFloat_impl(LLVMCodeGen *gen, AstNode *node) {
  if (!node || !node->val) {
    fprintf(stderr, "ERROR: Invalid float node\n");
    return NULL;
  }

  //  Use multi-format parser (supports decimal, scientific, hex floats)
  double value = parseFloat(node->val);
  return LLVMConstReal(gen->floatType, value);
}

// Compile string literal
LLVMValueRef LLVMCodeGen_compileString_impl(LLVMCodeGen *gen, AstNode *node) {
  if (!node || !node->val) {
    fprintf(stderr, "ERROR: Invalid string node\n");
    return NULL;
  }

  // Process escape sequences (\n, \t, \xHH, etc.)
  char *processedString = parseString(node->val);

  // Create global string constant with processed escapes
  LLVMValueRef strConst = LLVMBuildGlobalStringPtr(gen->builder, processedString, ".str");

  // Free the processed string (parseString allocates memory)
  free(processedString);

  // Return raw i8* string (not boxed) for normal string operations
  return strConst;
}

// Compile variable lookup
//  Handle mutable variables (load from alloca pointer)
LLVMValueRef LLVMCodeGen_compileVariable_impl(LLVMCodeGen *gen, AstNode *node) {
  if (!node || !node->val) {
    fprintf(stderr, "ERROR: Invalid variable node\n");
    return NULL;
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Looking up variable: %s\n", node->val);
    #endif
  }

  LLVMValueRef value = LLVMVariableMap_get(gen->variables, node->val);
  if (!value) {
    // CRITICAL FIX: Also check functions map for variable-stored closures
    // Closures with forward declarations are stored in functions map for recursion support
    value = LLVMVariableMap_get(gen->functions, node->val);
    if (!value) {
      fprintf(stderr, "ERROR: Undefined variable '%s' at line %d\n",
              node->val, node->lineNumber);
      return NULL;
    }
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Variable '%s' found in functions map (closure)\n", node->val);
      #endif
    }
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Variable found, checking type\n");
    #endif
  }

  //  Check if this is a pointer (mutable variable)
  // If it's a pointer type, we need to load the value
  LLVMTypeRef valueType = LLVMTypeOf(value);
  if (!valueType) {
    fprintf(stderr, "ERROR: Failed to get type of variable '%s'\n", node->val);
    return NULL;
  }
  
  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Variable type kind: %d\n", LLVMGetTypeKind(valueType));
    #endif
  }
  
  if (LLVMGetTypeKind(valueType) == LLVMPointerTypeKind) {
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Variable is a pointer, checking if alloca\n");
      #endif
    }

    //  fix: Check if this is an alloca instruction (mutable variable)
    // String pointers from functions like repeat() are NOT allocas
    if (LLVMIsAAllocaInst(value)) {
      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Pointer is an alloca, loading value\n");
        #endif
      }
      // This is a mutable variable (alloca pointer), load the current value
      // In LLVM 15+ (opaque pointers), use LLVMGetAllocatedType to get the type
      LLVMTypeRef pointeeType = LLVMGetAllocatedType(value);
      if (!pointeeType) {
        fprintf(stderr, "ERROR: Failed to get pointee type for '%s'\n", node->val);
        return NULL;
      }
      return LLVMBuildLoad2(gen->builder, pointeeType, value, node->val);
    } else {
      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Pointer is not an alloca (e.g., string), returning directly\n");
        #endif
      }
      // Not an alloca - this is a regular pointer value (e.g., string from repeat())
      // Return it directly without loading
      return value;
    }
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Returning immutable variable value\n");
    #endif
  }

  // Immutable variable, return the value directly
  return value;
}

// Compile variable assignment
//  Support mutable variables with alloca/store/load pattern
LLVMValueRef LLVMCodeGen_compileAssignment_impl(LLVMCodeGen *gen, AstNode *node) {
  #if 0  // Debug output disabled
  fprintf(stderr, "\n[ASSIGNMENT COMPILE START] ========== Starting assignment compilation ==========\n");
  #endif

  if (!node || node->childCount < 2) {
    fprintf(stderr, "ERROR: Invalid assignment node\n");
    return NULL;
  }

  // First child is the variable name (OP_IDENTIFIER)
  AstNode *varNode = node->children[0];
  #if 0  // Debug output disabled
  fprintf(stderr, "[ASSIGNMENT COMPILE] Variable name: %s\n", varNode ? varNode->val : "(null)");
  #endif

  if (!varNode || !varNode->val) {
    fprintf(stderr, "ERROR: Invalid assignment variable\n");
    return NULL;
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Compiling assignment: %s\n", varNode->val);
    #endif
  }

  // Second child is the value expression
  AstNode *valueNode = node->children[1];

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Compiling value expression (opcode: %d)\n", valueNode->opcode);
    #endif
  }

  //  If assigning a function, set currentFunctionName for recursion support
  const char *prevFunctionName = gen->currentFunctionName;
  if (valueNode->opcode == OP_FUNCTION) {
    gen->currentFunctionName = varNode->val;
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Setting currentFunctionName to '%s' for recursion support\n", varNode->val);
      #endif
    }
  }

  LLVMValueRef value = LLVMCodeGen_compileNode_impl(gen, valueNode);

  //  Restore previous function name after compilation
  if (valueNode->opcode == OP_FUNCTION) {
    gen->currentFunctionName = prevFunctionName;
  }

  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile assignment value\n");
    return NULL;
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Value compiled successfully\n");
    #endif
  }

  //  If assigning a function, store it in the function map
  //  TCO: Check if forward declaration exists first
  // If we created a forward declaration for this function, use it instead of creating a closure
  LLVMValueRef forwardDecl = LLVMVariableMap_get(gen->functions, varNode->val);

  #if 0  // Debug output disabled
  fprintf(stderr, "[ASSIGNMENT FUNCTIONS CHECK] Checking '%s': forwardDecl=%p, value=%p\n",
          varNode->val, forwardDecl, value);
  #endif
  #if 0  // Debug output disabled
  fprintf(stderr, "[ASSIGNMENT FUNCTIONS CHECK] valueNode->opcode=%d, OP_FUNCTION=%d\n",
          valueNode->opcode, OP_FUNCTION);
  #endif

  // CRITICAL FIX: If the value is a wrapped closure (closure_as_i64), DON'T store in functions map
  // Check if value name contains "closure_as_i64" - if so, it's already wrapped
  int isWrappedClosure = 0;
  if (value) {
    const char *valueName = LLVMGetValueName(value);
    if (valueName && strstr(valueName, "closure_as_i64")) {
      isWrappedClosure = 1;
      #if 0  // Debug output disabled
      fprintf(stderr, "[ASSIGNMENT FUNCTIONS CHECK] Value is wrapped closure, will NOT add to functions map\n");
      #endif
    }
  }

  if (valueNode->opcode == OP_FUNCTION && !isWrappedClosure && (!LLVMClosures_isClosure(valueNode) || forwardDecl)) {
    // Regular function OR has forward declaration (for recursion) - store in functions map
    // BUT NOT if it's already a wrapped closure
    if (forwardDecl && gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Using forward declaration for recursion: %s\n", varNode->val);
      #endif
    }
    #if 0  // Debug output disabled
    fprintf(stderr, "[ASSIGNMENT FUNCTIONS CHECK] Adding '%s' to functions map\n", varNode->val);
    #endif
    LLVMVariableMap_set(gen->functions, varNode->val, value);
  } else {
    #if 0  // Debug output disabled
    fprintf(stderr, "[ASSIGNMENT FUNCTIONS CHECK] NOT adding '%s' to functions map (isWrappedClosure=%d)\n",
            varNode->val, isWrappedClosure);
    #endif
    //  Check if this is a mutable variable declaration or reassignment
    LLVMValueRef existingVar = LLVMVariableMap_get(gen->variables, varNode->val);

    if (existingVar != NULL) {
      // Variable already exists - this is a reassignment
      LLVMTypeRef existingType = LLVMTypeOf(existingVar);

      if (LLVMGetTypeKind(existingType) == LLVMPointerTypeKind) {
        // Existing variable is mutable (alloca pointer) - store new value
        LLVMBuildStore(gen->builder, value, existingVar);
      } else {
        // Existing variable is immutable - error!
        fprintf(stderr, "ERROR: Cannot reassign immutable variable '%s' at line %d\n",
                varNode->val, node->lineNumber);
        fprintf(stderr, "Hint: Declare the variable with 'mut' to make it mutable: mut %s = ...\n",
                varNode->val);
        return NULL;
      }
    } else {
      // New variable declaration
      //  Closures need to be stored in alloca (like mutable vars)
      // because they're pointers that need to persist across statements

      // CRITICAL FIX Issue #2: Track ALL function definitions, not just closures
      // Functions without free variables also need tracking because in LLVM mode,
      // function calls can return boxed Generic* values that need unboxing in arithmetic
      // Previously: only tracked LLVMClosures_isClosure(valueNode) → missed regular functions
      // Now: track ANY function definition (OP_FUNCTION) regardless of free variables
      int isDirectClosure = (valueNode->opcode == OP_FUNCTION);  // Removed free variable check!

      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "\n[ASSIGNMENT DEBUG] ========== Assigning '%s' ==========\n", varNode->val);
        #endif
        #if 0  // Debug output disabled
        fprintf(stderr, "[ASSIGNMENT DEBUG] valueNode opcode = %d (OP_FUNCTION=%d, OP_APPLICATION=%d)\n",
                valueNode->opcode, OP_FUNCTION, OP_APPLICATION);
        #endif
        #if 0  // Debug output disabled
        fprintf(stderr, "[ASSIGNMENT DEBUG] isDirectClosure = %d\n", isDirectClosure);
        #endif
      }

      // FIX: Also check if the COMPILED VALUE is a closure pointer
      // This handles: add5 = (make_adder 5) where make_adder returns a closure
      // UPDATE: Closures are now returned as i64 (universal representation)
      // So we need to check for both pointer AND integer types from function calls
      //
      // ENHANCEMENT: Check returnTypeTags map to avoid marking INT/FLOAT returns as closures
      int isClosureValue = 0;
      if (value && valueNode->opcode == OP_APPLICATION) {
        // Check if this function has an inferred return type
        int shouldMarkAsClosure = 1;  // Default: assume closure

        if (valueNode->childCount > 0) {
          AstNode *funcNode = valueNode->children[0];
          if (funcNode->opcode == OP_IDENTIFIER) {
            const char *funcName = funcNode->val;
            void *storedTag = LLVMVariableMap_get(gen->returnTypeTags, funcName);

            if (gen->debugMode) {
              fprintf(stderr, "[DEBUG] Looking up return tag for '%s': storedTag=%p\n", funcName, storedTag);
            }

            if (storedTag) {
              // Function has inferred return type - check if it's INT/FLOAT
              int tag = (int)((intptr_t)storedTag - 1);
              if (gen->debugMode) {
                fprintf(stderr, "[DEBUG] Extracted tag for '%s': %d (storedTag=%p, after -1)\n", funcName, tag, storedTag);
              }
              if (tag == CLOSURE_RETURN_INT || tag == CLOSURE_RETURN_FLOAT || tag == CLOSURE_RETURN_VOID) {
                // This function returns a native type, NOT a closure
                shouldMarkAsClosure = 0;
                if (gen->debugMode) {
                  fprintf(stderr, "[ASSIGNMENT] Function '%s' returns INT/FLOAT/VOID (tag=%d) → NOT marking result as closure\n", funcName, tag);
                }
              } else if (gen->debugMode) {
                fprintf(stderr, "[ASSIGNMENT] Function '%s' returns POINTER/CLOSURE/DYNAMIC (tag=%d) → marking result as closure\n", funcName, tag);
              }
            }
          }
        }

        if (shouldMarkAsClosure) {
          // Any function call result could be a closure
          // Closures are encoded as i64 (ptrtoint from closure struct pointer)
          isClosureValue = 1;
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[ASSIGNMENT DEBUG] isClosureValue = 1 (OP_APPLICATION detected)\n");
            #endif
          }
        }
      }

      // CRITICAL FIX: Also check if the value is a wrapped closure (closure_as_i64)
      // This handles: test_fn = {v -> <- v} where the function gets wrapped
      int isWrappedClosureValue = 0;
      if (value && valueNode->opcode == OP_FUNCTION) {
        const char *valueName = LLVMGetValueName(value);
        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[ASSIGNMENT DEBUG] OP_FUNCTION detected, valueName = '%s'\n", valueName ? valueName : "(null)");
          #endif
        }

        // INDUSTRY STANDARD FIX: Check if function was wrapped by looking at forward declarations
        // If the function has a forward declaration, it was compiled as a closure
        LLVMValueRef forwardDecl = LLVMVariableMap_get(gen->functions, varNode->val);
        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[ASSIGNMENT DEBUG] forwardDecl for '%s' = %p\n", varNode->val, (void*)forwardDecl);
          #endif
        }

        if (forwardDecl != NULL) {
          // Function has forward declaration -> it was wrapped in closure struct
          isWrappedClosureValue = 1;
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[ASSIGNMENT DEBUG] isWrappedClosureValue = 1 (has forward declaration)\n");
            #endif
          }
        } else if (valueName && strstr(valueName, "closure")) {
          // Also check value name as backup
          isWrappedClosureValue = 1;
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[ASSIGNMENT DEBUG] isWrappedClosureValue = 1 (closure in name)\n");
            #endif
          }
        }
      }

      int isClosure = isDirectClosure || isClosureValue || isWrappedClosureValue;
      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[ASSIGNMENT DEBUG] Final: isClosure = %d, node->isMutable = %d\n", isClosure, node->isMutable);
        #endif
      }

      if (node->isMutable || isClosure) {
        // Mutable variable OR closure: create alloca and store initial value
        LLVMTypeRef valueType = LLVMTypeOf(value);
        LLVMValueRef alloca = LLVMBuildAlloca(gen->builder, valueType, varNode->val);

        // Store the initial value
        LLVMBuildStore(gen->builder, value, alloca);

        // Store the alloca pointer in the variable map
        // Future accesses will load from this pointer
        LLVMVariableMap_set(gen->variables, varNode->val, alloca);

        //  If it's a closure, mark it in the closures map
        // (Use dummy value - we only check if the key exists)
        if (isClosure) {
          LLVMVariableMap_set(gen->closures, varNode->val, value);  // value is just a marker
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[CLOSURE MAP DEBUG] Added '%s' to closures map (mutable path)\n", varNode->val);
            #endif
          }
        }
      } else {
        // Immutable variable: store value directly (SSA style)
        LLVMVariableMap_set(gen->variables, varNode->val, value);

        //  Also mark immutable closures in closures map
        if (isClosure) {
          LLVMVariableMap_set(gen->closures, varNode->val, value);  // value is just a marker
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[CLOSURE MAP DEBUG] Added '%s' to closures map (immutable path)\n", varNode->val);
            #endif
          }
        }
      }

      //  Track type metadata for type() function
      // Store the AST opcode of the value being assigned (cast as void*)
      // NOTE: Add 1 to opcode to avoid NULL when opcode==0 (OP_INT)
      // Special handling for OP_APPLICATION: Track return types of known functions
      int opcodeToStore = valueNode->opcode;

      if (valueNode->opcode == OP_APPLICATION && valueNode->childCount > 0) {
        // Function application - check if it's a known built-in with predictable return type
        AstNode *funcNode = valueNode->children[0];
        if (funcNode && funcNode->opcode == OP_IDENTIFIER && funcNode->val) {
          const char *funcName = funcNode->val;

          // Arithmetic functions return integers (unless mixed with floats)
          if (strcmp(funcName, "add") == 0 || strcmp(funcName, "subtract") == 0 ||
              strcmp(funcName, "multiply") == 0 || strcmp(funcName, "divide") == 0 ||
              strcmp(funcName, "remainder") == 0 || strcmp(funcName, "abs") == 0 ||
              strcmp(funcName, "min") == 0 || strcmp(funcName, "max") == 0 ||
              strcmp(funcName, "random_int") == 0) {
            opcodeToStore = OP_INT;  // Default to integer (may be promoted to float at runtime)
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[TYPE METADATA] Function '%s' inferred as returning integer\n", funcName);
              #endif
            }
          }
          // Math functions return floats
          else if (strcmp(funcName, "power") == 0 || strcmp(funcName, "sqrt") == 0 ||
                   strcmp(funcName, "floor") == 0 || strcmp(funcName, "ceil") == 0 ||
                   strcmp(funcName, "round") == 0 || strcmp(funcName, "random") == 0 ||
                   strcmp(funcName, "random_range") == 0) {
            opcodeToStore = OP_FLOAT;
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[TYPE METADATA] Function '%s' inferred as returning float\n", funcName);
              #endif
            }
          }
          // Void functions (side effects only) - use OP_INT with value 0 as dummy
          else if (strcmp(funcName, "random_seed") == 0) {
            opcodeToStore = OP_INT;  // random_seed returns 0 (dummy void value)
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[TYPE METADATA] Function '%s' inferred as returning int (void dummy)\n", funcName);
              #endif
            }
          }
          // String functions return strings
          else if (strcmp(funcName, "join") == 0 || strcmp(funcName, "repeat") == 0) {
            opcodeToStore = OP_STRING;
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[TYPE METADATA] Function '%s' inferred as returning string\n", funcName);
              #endif
            }
          }
          // List functions return lists
          else if (strcmp(funcName, "cons") == 0 || strcmp(funcName, "tail") == 0 ||
                   strcmp(funcName, "range") == 0 || strcmp(funcName, "map") == 0 ||
                   strcmp(funcName, "filter") == 0 || strcmp(funcName, "reduce") == 0) {
            opcodeToStore = OP_LIST;
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[TYPE METADATA] Function '%s' inferred as returning list\n", funcName);
              #endif
            }
          }
          // Otherwise, keep as OP_APPLICATION (will error in type() lookup)
        }
      }

      // Store inferred opcode + 1
      LLVMVariableMap_set(gen->typeMetadata, varNode->val, (LLVMValueRef)(intptr_t)(opcodeToStore + 1));
      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[TYPE METADATA] Stored opcode %d for variable '%s'\n",
                opcodeToStore, varNode->val);
        #endif
      }

      //  Track Generic* values (lists, list operations, dict operations, and closures)
      // IMPORTANT: Track for BOTH mutable and immutable variables
      if (valueNode->opcode == OP_LIST) {
        // Direct list literal: nums = [1, 2, 3]
        LLVMVariableMap_set(gen->genericVariables, varNode->val, (LLVMValueRef)1);
      } else if (valueNode->opcode == OP_APPLICATION && valueNode->childCount > 0) {
        AstNode *funcNode = valueNode->children[0];

        //  Check if it's a closure call (all closures now return Generic*)
        // This is the INDUSTRY-STANDARD FIX for polymorphic closure returns
        int isClosure = 0;
        if (funcNode->opcode == OP_IDENTIFIER) {
          const char *funcName = funcNode->val;
          #if 0  // Debug output disabled
          fprintf(stderr, "[ASSIGNMENT DEBUG] Checking if '%s' is a closure...\n", funcName);
          #endif
          isClosure = (LLVMVariableMap_get(gen->closures, funcName) != NULL);
          #if 0  // Debug output disabled
          fprintf(stderr, "[ASSIGNMENT DEBUG] isClosure (from closures map) = %d\n", isClosure);
          #endif

          // ENHANCEMENT: Check if closure returns Generic* or native type
          int shouldTrackAsGeneric = isClosure;  // Default: assume Generic*
          if (isClosure) {
            // Check returnTypeTags to see if it returns INT/FLOAT/VOID
            void *storedTag = LLVMVariableMap_get(gen->returnTypeTags, funcName);
            if (storedTag) {
              int tag = (int)((intptr_t)storedTag - 1);
              if (tag == CLOSURE_RETURN_INT || tag == CLOSURE_RETURN_FLOAT || tag == CLOSURE_RETURN_VOID) {
                // Closure returns native type, NOT Generic*
                shouldTrackAsGeneric = 0;
                if (gen->debugMode) {
                  fprintf(stderr, "[TRACKING] Closure '%s' returns INT/FLOAT/VOID (tag=%d) → NOT tracking result as Generic*\n", funcName, tag);
                }
              } else if (gen->debugMode) {
                fprintf(stderr, "[TRACKING] Closure '%s' returns POINTER/CLOSURE/DYNAMIC (tag=%d) → tracking result as Generic*\n", funcName, tag);
              }
            }
          }

          if (shouldTrackAsGeneric) {
            // ALL closure calls return Generic* (boxed values with runtime type)
            LLVMVariableMap_set(gen->genericVariables, varNode->val, (LLVMValueRef)1);

            //  FIX: Also track as closure for nested closure support
            // This enables factory patterns where closures return closures
            // The returned Generic* might contain a closure struct, so we need to track it
            // to enable calling it later: add100 = (make_adder 100); (add100 23)
            LLVMVariableMap_set(gen->closures, varNode->val, (LLVMValueRef)1);

            #if 0  // Debug output disabled
            fprintf(stderr, "[ASSIGNMENT DEBUG] Tracking variable '%s' as Generic* AND closure (from closure call '%s')\n",
                    varNode->val, funcName);
            #endif
          }

          // List operations and refs that return Generic* ( + 6.6 + 6.6.1)
          if (strcmp(funcName, "head") == 0 || strcmp(funcName, "tail") == 0 ||
              strcmp(funcName, "cons") == 0 || strcmp(funcName, "nth") == 0 ||
              strcmp(funcName, "list_files") == 0 || strcmp(funcName, "get") == 0 ||
              strcmp(funcName, "filter") == 0 || strcmp(funcName, "map") == 0 ||
              strcmp(funcName, "reduce") == 0 ||
              strcmp(funcName, "ref") == 0 || strcmp(funcName, "deref") == 0) {
            LLVMVariableMap_set(gen->genericVariables, varNode->val, (LLVMValueRef)1);
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[ASSIGNMENT DEBUG] Tracking variable '%s' as Generic* (from %s)\n",
                      varNode->val, funcName);
              #endif
            }
          }
          // Dict operations that return Generic*
          if (strcmp(funcName, "dict") == 0 || strcmp(funcName, "dict_get") == 0 ||
              strcmp(funcName, "dict_set") == 0 || strcmp(funcName, "dict_merge") == 0 ||
              strcmp(funcName, "dict_keys") == 0 || strcmp(funcName, "dict_values") == 0 ||
              strcmp(funcName, "dict_map") == 0 || strcmp(funcName, "dict_filter") == 0) {
            LLVMVariableMap_set(gen->genericVariables, varNode->val, (LLVMValueRef)1);
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[ASSIGNMENT DEBUG] Tracking variable '%s' as Generic* (from %s)\n",
                      varNode->val, funcName);
              #endif
            }
          }
        }
      }
    }
  }

  //  Track variables that hold the canonical void value
  if (gen->voidVariables) {
    int assignedVoid = 0;
    if (valueNode->opcode == OP_IDENTIFIER && valueNode->val) {
      if (strcmp(valueNode->val, "void") == 0) {
        assignedVoid = 1;
      } else {
        LLVMValueRef rhsVoidMarker = LLVMVariableMap_get(gen->voidVariables, valueNode->val);
        if (rhsVoidMarker) {
          assignedVoid = 1;
        }
      }
    }

    LLVMVariableMap_set(gen->voidVariables, varNode->val,
                        assignedVoid ? (LLVMValueRef)1 : NULL);
  }

  return value;
}

// ============================================================================
// Stdlib Function Handlers
// ============================================================================

// Compile (add a b ...) - supports variadic arguments
LLVMValueRef LLVMCodeGen_compileAdd_impl(LLVMCodeGen *gen, AstNode *node) {
  #if 0  // Debug output disabled
  fprintf(stderr, "[ADD] Entry: childCount=%d\n", node->childCount);
  #endif
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: add requires at least 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile first argument
  #if 0  // Debug output disabled
  fprintf(stderr, "[ADD] Compiling first operand...\n");
  #endif
  LLVMValueRef result = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  #if 0  // Debug output disabled
  fprintf(stderr, "[ADD] First operand compiled: %p\n", (void*)result);
  #endif
  if (!result) {
    fprintf(stderr, "ERROR: Failed to compile add operand\n");
    return NULL;
  }

  //  Auto-unbox if it's a Generic* from list operations
  #if 0  // Debug output disabled
  fprintf(stderr, "[ADD] Unboxing first operand...\n");
  #endif
  result = LLVMUnboxing_unboxForArithmetic(gen, result, node->children[0]);
  #if 0  // Debug output disabled
  fprintf(stderr, "[ADD] First operand unboxed: %p\n", (void*)result);
  #endif

  // Add each subsequent argument
  for (int i = 1; i < node->childCount; i++) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[ADD] Loop iteration %d: compiling operand...\n", i);
    #endif
    LLVMValueRef right = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
    #if 0  // Debug output disabled
    fprintf(stderr, "[ADD] Operand %d compiled: %p\n", i, (void*)right);
    #endif
    if (!right) {
      fprintf(stderr, "ERROR: Failed to compile add operand at position %d\n", i);
      return NULL;
    }

    //  Auto-unbox if it's a Generic* from list operations
    #if 0  // Debug output disabled
    fprintf(stderr, "[ADD] Unboxing operand %d...\n", i);
    #endif
    right = LLVMUnboxing_unboxForArithmetic(gen, right, node->children[i]);
    #if 0  // Debug output disabled
    fprintf(stderr, "[ADD] Operand %d unboxed: %p\n", i, (void*)right);
    #endif

    // Check types - if either is float, do FAdd, else IAdd
    #if 0  // Debug output disabled
    fprintf(stderr, "[ADD] Checking types for add operation...\n");
    #endif
    LLVMTypeRef leftType = LLVMTypeOf(result);
    LLVMTypeRef rightType = LLVMTypeOf(right);
    #if 0  // Debug output disabled
    fprintf(stderr, "[ADD] leftType=%p, rightType=%p\n", (void*)leftType, (void*)rightType);
    #endif

    if (leftType == gen->floatType || rightType == gen->floatType) {
      // Convert int to float if needed
      if (leftType == gen->intType) {
        result = LLVMBuildSIToFP(gen->builder, result, gen->floatType, "itof");
      }
      if (rightType == gen->intType) {
        right = LLVMBuildSIToFP(gen->builder, right, gen->floatType, "itof");
      }
      result = LLVMBuildFAdd(gen->builder, result, right, "fadd");
    } else {
      result = LLVMBuildAdd(gen->builder, result, right, "iadd");
    }
  }

  return result;
}

// Compile (subtract a b ...) - supports variadic arguments
LLVMValueRef LLVMCodeGen_compileSubtract_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: subtract requires at least 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile first argument
  LLVMValueRef result = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!result) return NULL;

  //  Auto-unbox if it's a Generic* from list operations
  result = LLVMUnboxing_unboxForArithmetic(gen, result, node->children[0]);

  // Subtract each subsequent argument
  for (int i = 1; i < node->childCount; i++) {
    LLVMValueRef right = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
    if (!right) return NULL;

    //  Auto-unbox if it's a Generic* from list operations
    right = LLVMUnboxing_unboxForArithmetic(gen, right, node->children[i]);

    LLVMTypeRef leftType = LLVMTypeOf(result);
    LLVMTypeRef rightType = LLVMTypeOf(right);

    if (leftType == gen->floatType || rightType == gen->floatType) {
      if (leftType == gen->intType) {
        result = LLVMBuildSIToFP(gen->builder, result, gen->floatType, "itof");
      }
      if (rightType == gen->intType) {
        right = LLVMBuildSIToFP(gen->builder, right, gen->floatType, "itof");
      }
      result = LLVMBuildFSub(gen->builder, result, right, "fsub");
    } else {
      result = LLVMBuildSub(gen->builder, result, right, "isub");
    }
  }

  return result;
}

// Compile (multiply a b ...) - supports variadic arguments
LLVMValueRef LLVMCodeGen_compileMultiply_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: multiply requires at least 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile first argument
  LLVMValueRef result = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!result) return NULL;

  //  Auto-unbox if it's a Generic* from list operations
  result = LLVMUnboxing_unboxForArithmetic(gen, result, node->children[0]);

  // Multiply each subsequent argument
  for (int i = 1; i < node->childCount; i++) {
    LLVMValueRef right = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
    if (!right) return NULL;

    //  Auto-unbox if it's a Generic* from list operations
    right = LLVMUnboxing_unboxForArithmetic(gen, right, node->children[i]);

    LLVMTypeRef leftType = LLVMTypeOf(result);
    LLVMTypeRef rightType = LLVMTypeOf(right);

    if (leftType == gen->floatType || rightType == gen->floatType) {
      if (leftType == gen->intType) {
        result = LLVMBuildSIToFP(gen->builder, result, gen->floatType, "itof");
      }
      if (rightType == gen->intType) {
        right = LLVMBuildSIToFP(gen->builder, right, gen->floatType, "itof");
      }
      result = LLVMBuildFMul(gen->builder, result, right, "fmul");
    } else {
      result = LLVMBuildMul(gen->builder, result, right, "imul");
    }
  }

  return result;
}

// Compile (divide a b ...) - supports variadic arguments with division by zero checking
LLVMValueRef LLVMCodeGen_compileDivide_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: divide requires at least 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile first argument
  LLVMValueRef result = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!result) return NULL;

  //  Auto-unbox if it's a Generic* from list operations
  result = LLVMUnboxing_unboxForArithmetic(gen, result, node->children[0]);

  // Divide by each subsequent argument
  for (int i = 1; i < node->childCount; i++) {
    LLVMValueRef divisor = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
    if (!divisor) return NULL;

    //  Auto-unbox if it's a Generic* from list operations
    divisor = LLVMUnboxing_unboxForArithmetic(gen, divisor, node->children[i]);

    LLVMTypeRef leftType = LLVMTypeOf(result);
    LLVMTypeRef rightType = LLVMTypeOf(divisor);

    // Division by zero check for integers (floats handle it gracefully with inf/nan)
    if (rightType == gen->intType) {
      // Check if divisor is a constant zero
      if (LLVMIsConstant(divisor)) {
        if (LLVMConstIntGetSExtValue(divisor) == 0) {
          fprintf(stderr, "ERROR: Division by zero at line %d\n", node->lineNumber);
          return NULL;
        }
      }
      // For non-constant divisors, we could add runtime checks here in the future
      // For now, LLVM will generate undefined behavior for division by zero
    }

    if (leftType == gen->floatType || rightType == gen->floatType) {
      if (leftType == gen->intType) {
        result = LLVMBuildSIToFP(gen->builder, result, gen->floatType, "itof");
      }
      if (rightType == gen->intType) {
        divisor = LLVMBuildSIToFP(gen->builder, divisor, gen->floatType, "itof");
      }
      result = LLVMBuildFDiv(gen->builder, result, divisor, "fdiv");
    } else {
      result = LLVMBuildSDiv(gen->builder, result, divisor, "idiv");
    }
  }

  return result;
}

// Compile (println arg1 arg2 ...)
// Helper: Check if AST node returns Generic* (list or list operation)
//  Now also checks if variable holds Generic* using tracking map
//  Made non-static for use in llvm_comparisons.c
int isGenericPointerNode(LLVMCodeGen *gen, AstNode *node) {
  if (!node) {
    #if 0  // Debug output disabled
    if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] Node is NULL\n");
    #endif
    return 0;
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[isGenericPointerNode] Checking node opcode=%d\n", node->opcode);
    #endif
  }

  // List literal
  if (node->opcode == OP_LIST) {
    #if 0  // Debug output disabled
    if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] OP_LIST → TRUE\n");
    #endif
    return 1;
  }

  // List operations and closure applications (function applications)
  if (node->opcode == OP_APPLICATION && node->childCount > 0) {
    AstNode *funcNode = node->children[0];
    if (funcNode->opcode == OP_IDENTIFIER) {
      const char *name = funcNode->val;
      if (gen->debugMode) {
        fprintf(stderr, "[isGenericPointerNode] Checking function: '%s'\n", name);
      }

      //  UPSTREAM TAGGING FIX - Stdlib Exclusion List
      // Industry-standard pattern: Explicitly list functions that return int/bool
      // This prevents stdlib functions from being misidentified as Generic* returns
      // Pattern used by: V8 (builtin optimizations), GHC (unboxing primitives),
      //                  LLVM (intrinsic recognition)
      //
      // These functions return native int/bool values, NOT Generic* pointers:
      const char *stdlib_int_funcs[] = {
        // Arithmetic operations
        "add", "subtract", "multiply", "divide", "remainder",
        "abs", "min", "max", "power", "sqrt",
        "floor", "ceil", "round",

        // Comparison operations
        "is", "less_than", "greater_than",

        // Logical operations
        "not", "and", "or",

        // Type checking predicates (return bool as int 0/1)
        "is_int", "is_float", "is_string", "is_list", "is_function",

        // List query functions (return int/bool)
        "length", "empty?",

        // Random number generation (int variant)
        "random_int",

        // File operations (return bool/int)
        "file_exists", "dir_exists", "is_directory",
        "file_size", "file_mtime",

        // Terminal dimensions (return int)
        "rows", "columns",

        NULL  // Sentinel value for end of array
      };

      // Check if function is in stdlib exclusion list
      for (int i = 0; stdlib_int_funcs[i] != NULL; i++) {
        if (strcmp(name, stdlib_int_funcs[i]) == 0) {
          #if 0  // Debug output disabled
          if (gen->debugMode) {
            fprintf(stderr, "[isGenericPointerNode] Stdlib int/bool function '%s' → FALSE (excluded)\n", name);
          }
          #endif
          return 0;  // ✅ Known int/bool-returning function - NOT Generic*
        }
      }

      //  TYPE INFERENCE INTEGRATION - Query inferred return type tags
      // Industry-standard pattern: Use type inference to determine if function returns Generic*
      // This bridges Franz's Hindley-Milner inference with LLVM codegen optimization
      // Pattern used by: OCaml (type-driven compilation), Haskell (unboxed types),
      //                  Rust (zero-cost abstractions via type system)
      //
      // Query returnTypeTags map for user-defined functions with inferred types
      void *storedTag = LLVMVariableMap_get(gen->returnTypeTags, name);
      if (storedTag) {
        // Decode tag (stored as +1 to avoid NULL)
        int tag = (int)((intptr_t)storedTag - 1);

        if (gen->debugMode) {
          fprintf(stderr, "[QUERY] Function '%s' has inferred return tag: %d\n", name, tag);
        }

        // INT (0) and FLOAT (1) returns are NOT Generic* - they're native values
        // Only POINTER (2), CLOSURE (3), and DYNAMIC (5) need Generic* path
        if (tag == CLOSURE_RETURN_INT || tag == CLOSURE_RETURN_FLOAT) {
          if (gen->debugMode) {
            fprintf(stderr, "[OPTIMIZE] '%s' returns INT/FLOAT → SKIP Generic* path\n", name);
          }
          return 0;  // ✅ Type inference proves this returns native int/float
        }

        // VOID (4) also doesn't need Generic* path
        if (tag == CLOSURE_RETURN_VOID) {
          if (gen->debugMode) {
            fprintf(stderr, "[OPTIMIZE] '%s' returns VOID → SKIP Generic* path\n", name);
          }
          return 0;  // ✅ Void returns don't need Generic* boxing
        }

        // POINTER (2), CLOSURE (3), DYNAMIC (5) DO need Generic* path
        if (gen->debugMode) {
          fprintf(stderr, "[] '%s' returns POINTER/CLOSURE/DYNAMIC → USE Generic* path\n", name);
        }
        return 1;  // ✅ Type inference proves this needs Generic* path
      }

      //  INDUSTRY-STANDARD FIX - Closure applications return Generic*
      // This makes direct closure calls work: (println (identity "hello"))
      // Previously only worked when stored: r = (identity "hello"); (println r)
      int isClosure = (LLVMVariableMap_get(gen->closures, name) != NULL);
      if (isClosure) {
        #if 0  // Debug output disabled
        if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] Closure application '%s' → TRUE\n", name);
        #endif
        return 1;
      }

      //  + 6.6 + 6.6.1: List operations and refs that return Generic*
      if (strcmp(name, "head") == 0 || strcmp(name, "tail") == 0 ||
          strcmp(name, "cons") == 0 || strcmp(name, "nth") == 0 ||
          strcmp(name, "list_files") == 0 || strcmp(name, "get") == 0 ||
          strcmp(name, "filter") == 0 || strcmp(name, "map") == 0 ||
          strcmp(name, "reduce") == 0 ||
          strcmp(name, "ref") == 0 || strcmp(name, "deref") == 0) {
        #if 0  // Debug output disabled
        if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] List/ref operation → TRUE\n");
        #endif
        return 1;
      }
      //  Dict operations that return Generic*
      if (strcmp(name, "dict") == 0 || strcmp(name, "dict_get") == 0 ||
          strcmp(name, "dict_set") == 0 || strcmp(name, "dict_merge") == 0 ||
          strcmp(name, "dict_keys") == 0 || strcmp(name, "dict_values") == 0 ||
          strcmp(name, "dict_map") == 0 || strcmp(name, "dict_filter") == 0) {
        #if 0  // Debug output disabled
        if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] Dict operation '%s' → TRUE\n", name);
        #endif
        return 1;
      }
    }
  }

  //  Check if variable holds Generic* pointer (tracked during assignment)
  if (node->opcode == OP_IDENTIFIER && node->val) {
    if (gen->debugMode) {
      fprintf(stderr, "[isGenericPointerNode] OP_IDENTIFIER: %s\n", node->val);
    }
    if (LLVMVariableMap_get(gen->genericVariables, node->val) != NULL) {
      if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] Variable '%s' tracked → TRUE\n", node->val);
      return 1;  // Variable was assigned from list/list operation
    } else {
      if (gen->debugMode) fprintf(stderr, "[isGenericPointerNode] Variable '%s' NOT tracked → FALSE\n", node->val);
    }
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[isGenericPointerNode] No match → FALSE\n");
    #endif
  }
  return 0;
}

static int LLVMCodeGen_printWithParamTag(LLVMCodeGen *gen,
                                         LLVMValueRef arg,
                                         LLVMValueRef tagValue,
                                         const char *intFormat,
                                         const char *floatFormat,
                                         const char *stringFormat,
                                         const char *closureLiteral,
                                         const char *labelPrefix,
                                         int sequence) {
  if (!tagValue) {
    return 0;
  }

  LLVMValueRef argAsInt = arg;
  LLVMTypeRef argType = LLVMTypeOf(arg);
  LLVMTypeKind argKind = LLVMGetTypeKind(argType);
  if (argKind == LLVMPointerTypeKind) {
    argAsInt = LLVMBuildPtrToInt(gen->builder, arg, gen->intType, "param_tag_ptr_to_i64");
  } else if (argType == gen->floatType) {
    argAsInt = LLVMBuildBitCast(gen->builder, arg, gen->intType, "param_tag_double_to_i64");
  }

  LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
  LLVMValueRef currentFunc = LLVMGetBasicBlockParent(currentBlock);

  char blockName[64];
  snprintf(blockName, sizeof(blockName), "%s_param_int_%d", labelPrefix, sequence);
  LLVMBasicBlockRef tagIntBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);
  snprintf(blockName, sizeof(blockName), "%s_param_float_%d", labelPrefix, sequence);
  LLVMBasicBlockRef tagFloatBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);
  snprintf(blockName, sizeof(blockName), "%s_param_ptr_%d", labelPrefix, sequence);
  LLVMBasicBlockRef tagPtrBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);
  snprintf(blockName, sizeof(blockName), "%s_param_closure_%d", labelPrefix, sequence);
  LLVMBasicBlockRef tagClosureBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);
  snprintf(blockName, sizeof(blockName), "%s_param_void_%d", labelPrefix, sequence);
  LLVMBasicBlockRef tagVoidBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);
  snprintf(blockName, sizeof(blockName), "%s_param_merge_%d", labelPrefix, sequence);
  LLVMBasicBlockRef tagMergeBlock = LLVMAppendBasicBlockInContext(gen->context, currentFunc, blockName);

  LLVMValueRef switchInst = LLVMBuildSwitch(gen->builder, tagValue, tagPtrBlock, 5);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_INT, 0), tagIntBlock);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_FLOAT, 0), tagFloatBlock);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_POINTER, 0), tagPtrBlock);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_CLOSURE, 0), tagClosureBlock);
  LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), CLOSURE_RETURN_VOID, 0), tagVoidBlock);

  // INT branch
  LLVMPositionBuilderAtEnd(gen->builder, tagIntBlock);
  char fmtLabel[64];
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_fmt_int_%d", labelPrefix, sequence);
  LLVMValueRef fmtInt = LLVMBuildGlobalStringPtr(gen->builder, intFormat, fmtLabel);
  LLVMValueRef printfArgsInt[] = {fmtInt, argAsInt};
  LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgsInt, 2, "printf_param_int");
  LLVMBuildBr(gen->builder, tagMergeBlock);

  // FLOAT branch
  LLVMPositionBuilderAtEnd(gen->builder, tagFloatBlock);
  LLVMValueRef floatValue = LLVMBuildBitCast(gen->builder, argAsInt, gen->floatType, "param_tag_i64_to_double");
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_fmt_float_%d", labelPrefix, sequence);
  LLVMValueRef fmtFloat = LLVMBuildGlobalStringPtr(gen->builder, floatFormat, fmtLabel);
  LLVMValueRef printfArgsFloat[] = {fmtFloat, floatValue};
  LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgsFloat, 2, "printf_param_float");
  LLVMBuildBr(gen->builder, tagMergeBlock);

  // POINTER branch
  LLVMPositionBuilderAtEnd(gen->builder, tagPtrBlock);
  LLVMValueRef ptrValue = LLVMBuildIntToPtr(gen->builder, argAsInt, gen->stringType, "param_tag_i64_to_ptr");
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_fmt_str_%d", labelPrefix, sequence);
  LLVMValueRef fmtStr = LLVMBuildGlobalStringPtr(gen->builder, stringFormat, fmtLabel);
  LLVMValueRef printfArgsStr[] = {fmtStr, ptrValue};
  LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgsStr, 2, "printf_param_str");
  LLVMBuildBr(gen->builder, tagMergeBlock);

  // CLOSURE branch
  LLVMPositionBuilderAtEnd(gen->builder, tagClosureBlock);
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_fmt_closure_%d", labelPrefix, sequence);
  LLVMValueRef fmtClosure = LLVMBuildGlobalStringPtr(gen->builder, "%s", fmtLabel);
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_param_closure_lit_%d", labelPrefix, sequence);
  LLVMValueRef literal = LLVMBuildGlobalStringPtr(gen->builder, closureLiteral, fmtLabel);
  LLVMValueRef printfArgsClosure[] = {fmtClosure, literal};
  LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgsClosure, 2, "printf_param_closure");
  LLVMBuildBr(gen->builder, tagMergeBlock);

  // VOID branch
  LLVMPositionBuilderAtEnd(gen->builder, tagVoidBlock);
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_fmt_void_%d", labelPrefix, sequence);
  LLVMValueRef fmtVoid = LLVMBuildGlobalStringPtr(gen->builder, "%s", fmtLabel);
  snprintf(fmtLabel, sizeof(fmtLabel), ".%s_param_void_lit_%d", labelPrefix, sequence);
  LLVMValueRef voidLiteral = LLVMBuildGlobalStringPtr(gen->builder, "void ", fmtLabel);
  LLVMValueRef printfArgsVoid[] = {fmtVoid, voidLiteral};
  LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgsVoid, 2, "printf_param_void");
  LLVMBuildBr(gen->builder, tagMergeBlock);

  LLVMPositionBuilderAtEnd(gen->builder, tagMergeBlock);
  return 1;
}

typedef struct {
  AstNode *node;
  LLVMValueRef value;
  LLVMValueRef paramTag;
} PrintlnArgInfo;

LLVMValueRef LLVMCodeGen_compilePrintln_impl(LLVMCodeGen *gen, AstNode *node) {
  LLVMValueRef lastCall = NULL;

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef printGenericFunc = LLVMGetNamedFunction(gen->module, "franz_print_generic");
  if (!printGenericFunc) {
    LLVMTypeRef params[] = { genericPtrType };
    LLVMTypeRef funcType = LLVMFunctionType(LLVMVoidTypeInContext(gen->context), params, 1, 0);
    printGenericFunc = LLVMAddFunction(gen->module, "franz_print_generic", funcType);
  }

  int argCount = node->childCount;
  if (argCount == 0) return NULL;

  PrintlnArgInfo *argInfos = (PrintlnArgInfo *)calloc(argCount, sizeof(PrintlnArgInfo));

  for (int i = 0; i < argCount; i++) {
    AstNode *childNode = node->children[i];
    argInfos[i].node = childNode;

    #if 0  // Debug output disabled
    fprintf(stderr, "[PRINTLN ARG DEBUG] Processing argument %d, opcode=%d\n", i, childNode->opcode);
    #endif
    if (childNode->opcode == OP_APPLICATION && childNode->childCount > 0) {
      AstNode *funcNode = childNode->children[0];
      if (funcNode->opcode == OP_IDENTIFIER) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[PRINTLN ARG DEBUG] Application of function: %s\n", funcNode->val);
        #endif
      }
    }

    LLVMValueRef arg = LLVMCodeGen_compileNode_impl(gen, childNode);
    if (!arg) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[PRINTLN ARG DEBUG] Compilation returned NULL!\n");
      #endif
      continue;
    }

    #if 0  // Debug output disabled
    fprintf(stderr, "[PRINTLN ARG DEBUG] Compiled successfully, type kind=%d\n",
            LLVMGetTypeKind(LLVMTypeOf(arg)));
    #endif

    argInfos[i].value = arg;
    if (childNode->opcode == OP_IDENTIFIER && gen->paramTypeTags) {
      argInfos[i].paramTag = LLVMVariableMap_get(gen->paramTypeTags, childNode->val);
    }
  }

  for (int i = 0; i < argCount; i++) {
    AstNode *childNode = argInfos[i].node;
    LLVMValueRef arg = argInfos[i].value;
    if (!childNode || !arg) continue;

    LLVMValueRef paramTag = argInfos[i].paramTag;
    if (paramTag) {
      if (LLVMCodeGen_printWithParamTag(gen, arg, paramTag,
                                        "%lld ", "%f ", "%s ",
                                        "<closure> ", "println", i)) {
        continue;
      }
    }

    LLVMTypeRef argType = LLVMTypeOf(arg);
    #if 0  // Debug output disabled
    fprintf(stderr, "[PRINTLN ARG DEBUG] Checking type handling for type kind=%d\n",
            LLVMGetTypeKind(argType));
    #endif
    #if 0  // Debug output disabled
    fprintf(stderr, "[PRINTLN ARG DEBUG] isGenericPointerNode=%d\n",
            isGenericPointerNode(gen, childNode));
    #endif

    if (isGenericPointerNode(gen, childNode)) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[PRINTLN ARG DEBUG] Taking Generic* path\n");
      #endif
      LLVMValueRef genericPtr = arg;
      if (LLVMGetTypeKind(argType) == LLVMIntegerTypeKind && argType == gen->intType) {
        genericPtr = LLVMBuildIntToPtr(gen->builder, arg, genericPtrType, "generic_ptr");
      }
      LLVMValueRef args[] = { genericPtr };
      LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(printGenericFunc),
                     printGenericFunc, args, 1, "");
      lastCall = NULL;
      continue;
    }

    if (argType == gen->intType) {
      LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%lld", ".fmt_int");
      LLVMValueRef printfArgs[] = {formatStr, arg};
      lastCall = LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 2, "printf_int");
    } else if (argType == gen->floatType) {
      LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%f", ".fmt_float");
      LLVMValueRef printfArgs[] = {formatStr, arg};
      lastCall = LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 2, "printf_float");
    } else if (LLVMGetTypeKind(argType) == LLVMPointerTypeKind) {
      LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%s", ".fmt_str");
      LLVMValueRef printfArgs[] = {formatStr, arg};
      lastCall = LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 2, "printf_str");
    }
  }

  // Add newline at the end (println should end with newline)
  LLVMValueRef newlineStr = LLVMBuildGlobalStringPtr(gen->builder, "\n", ".newline");
  LLVMValueRef printfArgs[] = {newlineStr};
  LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 1, "printf_newline");

  free(argInfos);
  return lastCall;
}


// Compile (print arg1 arg2 ...)
LLVMValueRef LLVMCodeGen_compilePrint_impl(LLVMCodeGen *gen, AstNode *node) {
  LLVMValueRef lastCall = NULL;

  for (int i = 0; i < node->childCount; i++) {
    LLVMValueRef arg = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
    if (!arg) continue;

    LLVMValueRef paramTag = NULL;
    if (node->children[i]->opcode == OP_IDENTIFIER && gen->paramTypeTags) {
      paramTag = LLVMVariableMap_get(gen->paramTypeTags, node->children[i]->val);
    }
    if (paramTag) {
      if (LLVMCodeGen_printWithParamTag(gen, arg, paramTag,
                                        "%lld", "%f", "%s",
                                        "<closure>", "print", i)) {
        continue;
      }
    }

    LLVMTypeRef argType = LLVMTypeOf(arg);

    if (argType == gen->intType) {
      LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%lld", ".fmt_int");
      LLVMValueRef printfArgs[] = {formatStr, arg};
      lastCall = LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 2, "printf_int");
    } else if (argType == gen->floatType) {
      LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%f", ".fmt_float");
      LLVMValueRef printfArgs[] = {formatStr, arg};
      lastCall = LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 2, "printf_float");
    } else if (LLVMGetTypeKind(argType) == LLVMPointerTypeKind) {
      LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%s", ".fmt_str");
      LLVMValueRef printfArgs[] = {formatStr, arg};
      lastCall = LLVMBuildCall2(gen->builder, gen->printfType, gen->printfFunc, printfArgs, 2, "printf_str");
    }
  }

  return lastCall;
}

// ============================================================================
//  I/O Functions
// ============================================================================

// Compile (input) - reads a line from stdin using getchar()
LLVMValueRef LLVMCodeGen_compileInput_impl(LLVMCodeGen *gen, AstNode *node) {
  // input takes no arguments
  if (node->childCount != 0) {
    fprintf(stderr, "ERROR: input requires 0 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Safety check: input() requires a current function context
  if (!gen->currentFunction) {
    fprintf(stderr, "ERROR: input() cannot be used at top-level (line %d)\n", node->lineNumber);
    fprintf(stderr, "Hint: Wrap input() call inside a function or use it in main body\n");
    return NULL;
  }

  // Verify malloc is available
  if (!gen->mallocFunc) {
    fprintf(stderr, "ERROR: malloc function not initialized\n");
    return NULL;
  }

  // Initial allocation: malloc(1) for empty string
  LLVMValueRef initialSize = LLVMConstInt(gen->intType, 1, 0);
  LLVMValueRef mallocArgs[] = {initialSize};
  LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                       mallocArgs, 1, "input_buffer");
  
  if (!buffer) {
    fprintf(stderr, "ERROR: Failed to build malloc call for input buffer\n");
    return NULL;
  }

  // Allocate stack variables for loop state
  LLVMValueRef lengthPtr = LLVMBuildAlloca(gen->builder, gen->intType, "input_length_ptr");
  if (!lengthPtr) {
    fprintf(stderr, "ERROR: Failed to allocate length pointer\n");
    return NULL;
  }
  
  LLVMValueRef bufferPtr = LLVMBuildAlloca(gen->builder, gen->stringType, "input_buffer_ptr");
  if (!bufferPtr) {
    fprintf(stderr, "ERROR: Failed to allocate buffer pointer\n");
    return NULL;
  }
  
  // Store null terminator: buffer[0] = '\0'
  LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
  LLVMBuildStore(gen->builder, nullChar, buffer);
  
  // Initialize length to 0
  LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
  LLVMBuildStore(gen->builder, zero, lengthPtr);
  
  // Initialize buffer pointer
  LLVMBuildStore(gen->builder, buffer, bufferPtr);
  
  // Create loop blocks
  LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlock(gen->currentFunction, "input_loop");
  LLVMBasicBlockRef bodyBlock = LLVMAppendBasicBlock(gen->currentFunction, "input_body");
  LLVMBasicBlockRef endBlock = LLVMAppendBasicBlock(gen->currentFunction, "input_end");
  
  // Branch to loop
  LLVMBuildBr(gen->builder, loopBlock);
  
  // Loop block: read character and check if newline or EOF
  LLVMPositionBuilderAtEnd(gen->builder, loopBlock);
  
  // Call getchar()
  LLVMValueRef charInt = LLVMBuildCall2(gen->builder, gen->getcharType, gen->getcharFunc,
                                        NULL, 0, "char");
  
  // Check if newline (10) or EOF (-1)
  LLVMValueRef newline = LLVMConstInt(LLVMInt32TypeInContext(gen->context), 10, 0);
  LLVMValueRef eof = LLVMConstInt(LLVMInt32TypeInContext(gen->context), -1, 1);
  
  LLVMValueRef isNewline = LLVMBuildICmp(gen->builder, LLVMIntEQ, charInt, newline, "is_newline");
  LLVMValueRef isEof = LLVMBuildICmp(gen->builder, LLVMIntEQ, charInt, eof, "is_eof");
  LLVMValueRef shouldStop = LLVMBuildOr(gen->builder, isNewline, isEof, "should_stop");
  
  LLVMBuildCondBr(gen->builder, shouldStop, endBlock, bodyBlock);
  
  // Body block: append character to buffer
  LLVMPositionBuilderAtEnd(gen->builder, bodyBlock);
  
  // Load current buffer and length
  LLVMValueRef currentBuffer = LLVMBuildLoad2(gen->builder, gen->stringType, bufferPtr, "current_buffer");
  LLVMValueRef currentLength = LLVMBuildLoad2(gen->builder, gen->intType, lengthPtr, "current_length");
  
  // New length = currentLength + 1
  LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);
  LLVMValueRef newLength = LLVMBuildAdd(gen->builder, currentLength, one, "new_length");
  
  // New size = newLength + 1 (for null terminator)
  LLVMValueRef newSize = LLVMBuildAdd(gen->builder, newLength, one, "new_size");
  
  // Reallocate buffer
  LLVMValueRef reallocArgs[] = {currentBuffer, newSize};
  LLVMValueRef newBuffer = LLVMBuildCall2(gen->builder, gen->reallocType, gen->reallocFunc,
                                          reallocArgs, 2, "new_buffer");
  
  // Convert charInt (i32) to i8
  LLVMValueRef charByte = LLVMBuildTrunc(gen->builder, charInt, LLVMInt8TypeInContext(gen->context), "char_byte");
  
  // Store character at buffer[currentLength]
  LLVMValueRef charPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                       newBuffer, &currentLength, 1, "char_ptr");
  LLVMBuildStore(gen->builder, charByte, charPtr);
  
  // Store null terminator at buffer[newLength]
  LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                       newBuffer, &newLength, 1, "null_ptr");
  LLVMBuildStore(gen->builder, nullChar, nullPtr);
  
  // Update buffer pointer and length
  LLVMBuildStore(gen->builder, newBuffer, bufferPtr);
  LLVMBuildStore(gen->builder, newLength, lengthPtr);
  
  // Continue loop
  LLVMBuildBr(gen->builder, loopBlock);
  
  // End block: return buffer
  LLVMPositionBuilderAtEnd(gen->builder, endBlock);
  LLVMValueRef finalBuffer = LLVMBuildLoad2(gen->builder, gen->stringType, bufferPtr, "final_buffer");
  
  return finalBuffer;
}

// ============================================================================
//  Type Conversion Functions
// ============================================================================

// Compile (integer x) - converts string/float/int to integer
LLVMValueRef LLVMCodeGen_compileInteger_func_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: integer requires 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef arg = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!arg) {
    fprintf(stderr, "ERROR: Failed to compile integer argument\n");
    return NULL;
  }

  LLVMTypeRef argType = LLVMTypeOf(arg);

  // If already int, return as-is (convert from i32 to i64 if needed)
  if (argType == gen->intType) {
    return arg;
  } else if (argType == LLVMInt32TypeInContext(gen->context)) {
    return LLVMBuildSExt(gen->builder, arg, gen->intType, "int_extend");
  }
  // If float, truncate to int
  else if (argType == gen->floatType) {
    return LLVMBuildFPToSI(gen->builder, arg, gen->intType, "ftoi");
  }
  // If string, use atoi
  else if (LLVMGetTypeKind(argType) == LLVMPointerTypeKind) {
    LLVMValueRef atoiArgs[] = {arg};
    LLVMValueRef result = LLVMBuildCall2(gen->builder, gen->atoiType, gen->atoiFunc,
                                         atoiArgs, 1, "atoi_call");
    // atoi returns i32, extend to i64
    return LLVMBuildSExt(gen->builder, result, gen->intType, "int_extend");
  }
  else {
    fprintf(stderr, "ERROR: integer() requires int, float, or string argument\n");
    return NULL;
  }
}

// Compile (float x) - converts string/int/float to float
LLVMValueRef LLVMCodeGen_compileFloat_func_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: float requires 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef arg = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!arg) {
    fprintf(stderr, "ERROR: Failed to compile float argument\n");
    return NULL;
  }

  LLVMTypeRef argType = LLVMTypeOf(arg);

  // If already float, return as-is
  if (argType == gen->floatType) {
    return arg;
  }
  // If int, convert to float
  else if (argType == gen->intType || argType == LLVMInt32TypeInContext(gen->context)) {
    return LLVMBuildSIToFP(gen->builder, arg, gen->floatType, "itof");
  }
  // If string, use atof
  else if (LLVMGetTypeKind(argType) == LLVMPointerTypeKind) {
    LLVMValueRef atofArgs[] = {arg};
    return LLVMBuildCall2(gen->builder, gen->atofType, gen->atofFunc,
                          atofArgs, 1, "atof_call");
  }
  else {
    fprintf(stderr, "ERROR: float() requires int, float, or string argument\n");
    return NULL;
  }
}

// Compile (string x) - converts int/float/string to string
LLVMValueRef LLVMCodeGen_compileString_func_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: string requires 1 argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMValueRef arg = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!arg) {
    fprintf(stderr, "ERROR: Failed to compile string argument\n");
    return NULL;
  }

  LLVMTypeRef argType = LLVMTypeOf(arg);

  // If already string (pointer), return as-is
  if (LLVMGetTypeKind(argType) == LLVMPointerTypeKind) {
    return arg;
  }
  // If int, convert to string using snprintf
  else if (argType == gen->intType) {
    // Allocate buffer on stack (50 bytes enough for any 64-bit int)
    LLVMValueRef bufferSize = LLVMConstInt(LLVMInt64TypeInContext(gen->context), 50, 0);
    LLVMValueRef buffer = LLVMBuildArrayAlloca(gen->builder,
                                               LLVMInt8TypeInContext(gen->context),
                                               bufferSize, "int_str_buffer");

    // Call snprintf(buffer, 50, "%lld", arg)
    LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%lld", ".fmt_lld");
    LLVMValueRef snprintfArgs[] = {buffer, bufferSize, formatStr, arg};
    LLVMBuildCall2(gen->builder, gen->snprintfType, gen->snprintfFunc,
                   snprintfArgs, 4, "snprintf_int");

    return buffer;
  }
  // If float, convert to string using snprintf
  else if (argType == gen->floatType) {
    // Allocate buffer on stack (50 bytes enough for any double)
    LLVMValueRef bufferSize = LLVMConstInt(LLVMInt64TypeInContext(gen->context), 50, 0);
    LLVMValueRef buffer = LLVMBuildArrayAlloca(gen->builder,
                                               LLVMInt8TypeInContext(gen->context),
                                               bufferSize, "float_str_buffer");

    // Call snprintf(buffer, 50, "%f", arg)
    LLVMValueRef formatStr = LLVMBuildGlobalStringPtr(gen->builder, "%f", ".fmt_f");
    LLVMValueRef snprintfArgs[] = {buffer, bufferSize, formatStr, arg};
    LLVMBuildCall2(gen->builder, gen->snprintfType, gen->snprintfFunc,
                   snprintfArgs, 4, "snprintf_float");

    return buffer;
  }
  else {
    fprintf(stderr, "ERROR: string() requires int, float, or string argument\n");
    return NULL;
  }
}

//  Compile (format-int value base)
LLVMValueRef LLVMCodeGen_compileFormatInt_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: format-int requires 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile value (first argument)
  LLVMValueRef value = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile format-int value\n");
    return NULL;
  }

  // Compile base (second argument)
  LLVMValueRef base = LLVMCodeGen_compileNode_impl(gen, node->children[1]);
  if (!base) {
    fprintf(stderr, "ERROR: Failed to compile format-int base\n");
    return NULL;
  }

  // Ensure value is i64
  LLVMTypeRef valueType = LLVMTypeOf(value);
  if (valueType != gen->intType) {
    fprintf(stderr, "ERROR: format-int value must be integer\n");
    return NULL;
  }

  // Ensure base is i32 (or convert from i64)
  LLVMTypeRef baseType = LLVMTypeOf(base);
  if (baseType == gen->intType) {
    // Truncate i64 to i32
    base = LLVMBuildTrunc(gen->builder, base, LLVMInt32TypeInContext(gen->context), "base_i32");
  }

  // Call formatInteger(value, base)
  LLVMValueRef args[] = {value, base};
  return LLVMBuildCall2(gen->builder, gen->formatIntegerType, gen->formatIntegerFunc,
                        args, 2, "format_int_result");
}

//  Compile (format-float value precision)
LLVMValueRef LLVMCodeGen_compileFormatFloat_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: format-float requires 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile value (first argument)
  LLVMValueRef value = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!value) {
    fprintf(stderr, "ERROR: Failed to compile format-float value\n");
    return NULL;
  }

  // Compile precision (second argument)
  LLVMValueRef precision = LLVMCodeGen_compileNode_impl(gen, node->children[1]);
  if (!precision) {
    fprintf(stderr, "ERROR: Failed to compile format-float precision\n");
    return NULL;
  }

  // Convert value to double if it's an integer
  LLVMTypeRef valueType = LLVMTypeOf(value);
  if (valueType == gen->intType) {
    value = LLVMBuildSIToFP(gen->builder, value, gen->floatType, "int_to_float");
  } else if (valueType != gen->floatType) {
    fprintf(stderr, "ERROR: format-float value must be int or float\n");
    return NULL;
  }

  // Ensure precision is i32 (or convert from i64)
  LLVMTypeRef precisionType = LLVMTypeOf(precision);
  if (precisionType == gen->intType) {
    // Truncate i64 to i32
    precision = LLVMBuildTrunc(gen->builder, precision, LLVMInt32TypeInContext(gen->context), "precision_i32");
  }

  // Call formatFloat(value, precision)
  LLVMValueRef args[] = {value, precision};
  return LLVMBuildCall2(gen->builder, gen->formatFloatType, gen->formatFloatFunc,
                        args, 2, "format_float_result");
}

// ============================================================================
//  String Manipulation Functions
// ============================================================================

// Compile (join str1 str2 ...) - concatenate strings (variadic)
// Implements the same behavior as StdLib_join from stdlib.c
LLVMValueRef LLVMCodeGen_compileJoin_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 2) {
    fprintf(stderr, "ERROR: join requires at least 2 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile all arguments
  LLVMValueRef *strings = malloc(node->childCount * sizeof(LLVMValueRef));
  LLVMValueRef *lengths = malloc(node->childCount * sizeof(LLVMValueRef));

  // Compile each string argument
  for (int i = 0; i < node->childCount; i++) {
    strings[i] = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
    if (!strings[i]) {
      free(strings);
      free(lengths);
      return NULL;
    }

    // Verify it's a string type
    LLVMTypeRef argType = LLVMTypeOf(strings[i]);
    if (argType != gen->stringType) {
      fprintf(stderr, "ERROR: join argument %d must be a string at line %d\n", i + 1, node->lineNumber);
      free(strings);
      free(lengths);
      return NULL;
    }

    // Calculate length of this string
    LLVMValueRef strlenArgs[] = {strings[i]};
    lengths[i] = LLVMBuildCall2(gen->builder, gen->strlenType, gen->strlenFunc,
                                strlenArgs, 1, "strlen");
  }

  // Calculate total size needed (sum of all lengths + 1 for null terminator)
  LLVMValueRef totalSize = lengths[0];
  for (int i = 1; i < node->childCount; i++) {
    totalSize = LLVMBuildAdd(gen->builder, totalSize, lengths[i], "add_len");
  }
  // Add 1 for null terminator
  totalSize = LLVMBuildAdd(gen->builder, totalSize, LLVMConstInt(gen->intType, 1, 0), "add_null");

  // Allocate memory for result
  LLVMValueRef mallocArgs[] = {totalSize};
  LLVMValueRef result = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                       mallocArgs, 1, "join_result");

  // Copy first string to result buffer
  LLVMValueRef strcpyArgs[] = {result, strings[0]};
  LLVMBuildCall2(gen->builder, gen->strcpyType, gen->strcpyFunc,
                 strcpyArgs, 2, "strcpy");

  // Concatenate remaining strings
  for (int i = 1; i < node->childCount; i++) {
    LLVMValueRef strcatArgs[] = {result, strings[i]};
    LLVMBuildCall2(gen->builder, gen->strcatType, gen->strcatFunc,
                   strcatArgs, 2, "strcat");
  }

  free(strings);
  free(lengths);

  return result;
}

// ============================================================================
//  Get Function (Substring/List Indexing)
// ============================================================================

// Compile (get collection index) or (get collection start end)
// Delegates to LLVM-native string ops implementation for maximum performance
LLVMValueRef LLVMCodeGen_compileGet_impl(LLVMCodeGen *gen, AstNode *node) {
  return LLVMStringOps_compileGet(gen, node);
}

// OLD IMPLEMENTATION - Replaced by LLVM-native version in llvm-string-ops
/*
LLVMValueRef LLVMCodeGen_compileGet_impl_OLD(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount < 2 || node->childCount > 3) {
    fprintf(stderr, "ERROR: get requires 2 or 3 arguments at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile collection/string (first argument)
  LLVMValueRef collection = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!collection) {
    fprintf(stderr, "ERROR: Failed to compile get collection argument at line %d\n", node->lineNumber);
    return NULL;
  }

  // Compile index/start (second argument)
  LLVMValueRef index = LLVMCodeGen_compileNode_impl(gen, node->children[1]);
  if (!index) {
    fprintf(stderr, "ERROR: Failed to compile get index argument at line %d\n", node->lineNumber);
    return NULL;
  }

  LLVMTypeRef collectionType = LLVMTypeOf(collection);
  LLVMTypeKind collectionKind = LLVMGetTypeKind(collectionType);

  // Check if collection is a Generic* (list), string, or i64 (Universal Type)
  if (collectionKind == LLVMPointerTypeKind) {
    // String operation (native LLVM)
    if (collectionType == gen->stringType) {
      if (node->childCount == 2) {
        // Single character: (get "hello" 0) → "h"
        LLVMValueRef two = LLVMConstInt(gen->intType, 2, 0);
        LLVMValueRef mallocArgs[] = {two};
        LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                             mallocArgs, 1, "char_buffer");

        // Load character at index from string
        LLVMValueRef charPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             collection, &index, 1, "char_ptr");
        LLVMValueRef charValue = LLVMBuildLoad2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                                charPtr, "char_value");

        // Store character in buffer[0]
        LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
        LLVMValueRef bufferCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                                   buffer, &zero, 1, "buffer_char_ptr");
        LLVMBuildStore(gen->builder, charValue, bufferCharPtr);

        // Store null terminator in buffer[1]
        LLVMValueRef one = LLVMConstInt(gen->intType, 1, 0);
        LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             buffer, &one, 1, "null_ptr");
        LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
        LLVMBuildStore(gen->builder, nullChar, nullPtr);

        return buffer;
      } else {
        // Substring: (get "hello" 0 3) → "hel"
        LLVMValueRef end = LLVMCodeGen_compileNode_impl(gen, node->children[2]);
        if (!end) {
          fprintf(stderr, "ERROR: Failed to compile get end argument at line %d\n", node->lineNumber);
          return NULL;
        }

        // Calculate substring length: end - start
        LLVMValueRef length = LLVMBuildSub(gen->builder, end, index, "sub_length");

        // Allocate buffer: length + 1 (for null terminator)
        LLVMValueRef bufferSize = LLVMBuildAdd(gen->builder, length,
                                               LLVMConstInt(gen->intType, 1, 0), "buffer_size");
        LLVMValueRef mallocArgs[] = {bufferSize};
        LLVMValueRef buffer = LLVMBuildCall2(gen->builder, gen->mallocType, gen->mallocFunc,
                                             mallocArgs, 1, "substring_buffer");

        // Copy characters from source to buffer using a loop
        // Create loop: for (i = 0; i < length; i++) buffer[i] = src[start+i]
        LLVMBasicBlockRef loopBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "substr_loop");
        LLVMBasicBlockRef endBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "substr_end");

        // Allocate loop counter
        LLVMValueRef counter = LLVMBuildAlloca(gen->builder, gen->intType, "loop_counter");
        LLVMValueRef zero = LLVMConstInt(gen->intType, 0, 0);
        LLVMBuildStore(gen->builder, zero, counter);

        // Branch to loop
        LLVMBuildBr(gen->builder, loopBlock);

        // Loop body
        LLVMPositionBuilderAtEnd(gen->builder, loopBlock);
        LLVMValueRef i = LLVMBuildLoad2(gen->builder, gen->intType, counter, "i");

        // Check if i < length
        LLVMValueRef cond = LLVMBuildICmp(gen->builder, LLVMIntSLT, i, length, "loop_cond");
        LLVMBasicBlockRef bodyBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "body");
        LLVMBuildCondBr(gen->builder, cond, bodyBlock, endBlock);

        // Copy character
        LLVMPositionBuilderAtEnd(gen->builder, bodyBlock);
        LLVMValueRef srcIndex = LLVMBuildAdd(gen->builder, index, i, "src_index");
        LLVMValueRef srcCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                                collection, &srcIndex, 1, "src_char_ptr");
        LLVMValueRef charValue = LLVMBuildLoad2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                                srcCharPtr, "char_value");

        LLVMValueRef dstCharPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                                buffer, &i, 1, "dst_char_ptr");
        LLVMBuildStore(gen->builder, charValue, dstCharPtr);

        // Increment counter
        LLVMValueRef next = LLVMBuildAdd(gen->builder, i, LLVMConstInt(gen->intType, 1, 0), "next");
        LLVMBuildStore(gen->builder, next, counter);
        LLVMBuildBr(gen->builder, loopBlock);

        // End block: add null terminator
        LLVMPositionBuilderAtEnd(gen->builder, endBlock);
        LLVMValueRef nullPtr = LLVMBuildGEP2(gen->builder, LLVMInt8TypeInContext(gen->context),
                                             buffer, &length, 1, "null_ptr");
        LLVMValueRef nullChar = LLVMConstInt(LLVMInt8TypeInContext(gen->context), 0, 0);
        LLVMBuildStore(gen->builder, nullChar, nullPtr);

        return buffer;
      }
    } else {
      // Generic* list - use runtime function franz_list_nth
      LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
      LLVMValueRef listNthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
      if (!listNthFunc) {
        LLVMTypeRef listNthParams[] = {genericPtrType, gen->intType};
        LLVMTypeRef listNthType = LLVMFunctionType(genericPtrType, listNthParams, 2, 0);
        listNthFunc = LLVMAddFunction(gen->module, "franz_list_nth", listNthType);
      }

      LLVMValueRef args[] = {collection, index};
      return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNthFunc),
                            listNthFunc, args, 2, "list_nth");
    }
  } else if (collectionKind == LLVMIntegerTypeKind) {
    // i64 (Universal Type System) - could be Generic* list
    // Convert to ptr and call runtime function franz_list_nth
    LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
    LLVMValueRef listPtr = LLVMBuildIntToPtr(gen->builder, collection, genericPtrType, "list_ptr");

    LLVMValueRef listNthFunc = LLVMGetNamedFunction(gen->module, "franz_list_nth");
    if (!listNthFunc) {
      LLVMTypeRef listNthParams[] = {genericPtrType, gen->intType};
      LLVMTypeRef listNthType = LLVMFunctionType(genericPtrType, listNthParams, 2, 0);
      listNthFunc = LLVMAddFunction(gen->module, "franz_list_nth", listNthType);
    }

    LLVMValueRef args[] = {listPtr, index};
    return LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNthFunc),
                          listNthFunc, args, 2, "list_nth");
  } else {
    fprintf(stderr, "ERROR: get requires list or string as first argument at line %d\n", node->lineNumber);
    return NULL;
  }
}
*/

// ============================================================================
// Application Node Handler (Function Calls)
// ============================================================================

LLVMValueRef LLVMCodeGen_compileApplication_impl(LLVMCodeGen *gen, AstNode *node) {
  if (node->childCount == 0) {
    fprintf(stderr, "ERROR: Empty application at line %d\n", node->lineNumber);
    return NULL;
  }

  // First child is the function name or expression
  AstNode *funcNode = node->children[0];

  // Create argument node (remaining children)
  AstNode argNode;
  argNode.children = node->children + 1;
  argNode.childCount = node->childCount - 1;
  argNode.lineNumber = node->lineNumber;

  // Check if it's an identifier (function name)
  if (funcNode->opcode == OP_IDENTIFIER) {
    const char *funcName = funcNode->val;

    //  Module System - Handle use(), use_as(), use_with()
    if (strcmp(funcName, "use") == 0) {
      // use(module_path, callback) - Load module and execute callback
      // Syntax: (use "path/to/module.franz" {body})

      // Validate argument count (need at least module path, callback is optional)
      if (argNode.childCount < 1) {
        fprintf(stderr, "ERROR: use() requires at least 1 argument (module path) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // Extract module path from first argument (must be string literal)
      AstNode *pathNode = argNode.children[0];
      if (pathNode->opcode != OP_STRING) {
        fprintf(stderr, "ERROR: use() first argument must be a string literal (module path) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      const char *modulePath = pathNode->val;

      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Compiling use() for module '%s' at line %d\n", modulePath, node->lineNumber);
        #endif
      }

      // Load and compile the module
      if (LLVMModules_use(gen, modulePath, node->lineNumber) != 0) {
        fprintf(stderr, "ERROR: Failed to load module '%s' at line %d\n", modulePath, node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // If there's a callback, compile and execute it
      if (argNode.childCount >= 2) {
        AstNode *callbackNode = argNode.children[1];

        // Compile the callback (should be a function literal)
        LLVMValueRef callbackValue = LLVMCodeGen_compileNode_impl(gen, callbackNode);
        if (!callbackValue) {
          fprintf(stderr, "ERROR: Failed to compile use() callback at line %d\n", node->lineNumber);
          return LLVMConstInt(gen->intType, 0, 0);
        }

        // If the callback is a closure, call it
        // Check if it's marked as a closure
        int isClosureCallback = 0;
        if (callbackNode->opcode == OP_FUNCTION) {
          // It's a function literal - compile and call it
          // The function has already been compiled, now we need to call it
          // For LLVM mode, function literals create closures
          isClosureCallback = 1;
        }

        if (isClosureCallback) {
          // Call the closure with 0 arguments
          LLVMValueRef *emptyArgs = NULL;
          LLVMValueRef result = LLVMClosures_callClosure(gen, callbackValue, emptyArgs, 0, NULL);
          return result ? result : LLVMConstInt(gen->intType, 0, 0);
        } else {
          // Just return the callback value
          return callbackValue;
        }
      }

      // No callback - just return void
      return LLVMConstInt(gen->intType, 0, 0);
    }

    if (strcmp(funcName, "use_as") == 0) {
      // use_as(module_path, namespace_name) - Load module with namespace prefix
      // Syntax: (use_as "path/to/module.franz" "namespace")

      // Validate argument count
      if (argNode.childCount < 2) {
        fprintf(stderr, "ERROR: use_as() requires 2 arguments (module path, namespace name) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // Extract module path from first argument (must be string literal)
      AstNode *pathNode = argNode.children[0];
      if (pathNode->opcode != OP_STRING) {
        fprintf(stderr, "ERROR: use_as() first argument must be a string literal (module path) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // Extract namespace name from second argument (must be string literal)
      AstNode *namespaceNode = argNode.children[1];
      if (namespaceNode->opcode != OP_STRING) {
        fprintf(stderr, "ERROR: use_as() second argument must be a string literal (namespace name) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      const char *modulePath = pathNode->val;
      const char *namespaceName = namespaceNode->val;

      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Compiling use_as() for module '%s' with namespace '%s' at line %d\n",
                modulePath, namespaceName, node->lineNumber);
        #endif
      }

      // Load and compile the module with namespace prefix
      if (LLVMModules_useAs(gen, modulePath, namespaceName, node->lineNumber) != 0) {
        fprintf(stderr, "ERROR: Failed to load module '%s' with namespace '%s' at line %d\n",
                modulePath, namespaceName, node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // Return void
      return LLVMConstInt(gen->intType, 0, 0);
    }

    if (strcmp(funcName, "use_with") == 0) {
      // use_with(capabilities, module_path1, module_path2, ...) - Load modules with capability restrictions
      // Syntax: (use_with (list "io" "math") "path/to/module.franz")

      // Validate argument count (need at least capabilities list + 1 module path)
      if (argNode.childCount < 2) {
        fprintf(stderr, "ERROR: use_with() requires at least 2 arguments (capabilities list, module path) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // Extract capabilities from first argument (must be a list literal)
      AstNode *capListNode = argNode.children[0];
      if (capListNode->opcode != OP_LIST) {
        fprintf(stderr, "ERROR: use_with() first argument must be a list literal (capabilities) at line %d\n", node->lineNumber);
        return LLVMConstInt(gen->intType, 0, 0);
      }

      // Extract capability strings from list
      int capCount = capListNode->childCount;
      const char **capabilities = malloc(capCount * sizeof(char*));

      for (int i = 0; i < capCount; i++) {
        if (capListNode->children[i]->opcode != OP_STRING) {
          fprintf(stderr, "ERROR: use_with() capability list must contain only strings at line %d\n", node->lineNumber);
          free(capabilities);
          return LLVMConstInt(gen->intType, 0, 0);
        }
        capabilities[i] = capListNode->children[i]->val;
      }

      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Compiling use_with() with %d capabilities at line %d\n",
                capCount, node->lineNumber);
        #endif
        for (int i = 0; i < capCount; i++) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[DEBUG]   - %s\n", capabilities[i]);
          #endif
        }
      }

      // Load each module with the specified capabilities
      for (int i = 1; i < argNode.childCount; i++) {
        AstNode *pathNode = argNode.children[i];

        if (pathNode->opcode != OP_STRING) {
          fprintf(stderr, "ERROR: use_with() module paths must be string literals at line %d\n", node->lineNumber);
          free(capabilities);
          return LLVMConstInt(gen->intType, 0, 0);
        }

        const char *modulePath = pathNode->val;

        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[DEBUG] Loading module '%s' with restricted capabilities at line %d\n",
                  modulePath, node->lineNumber);
          #endif
        }

        // Load and compile the module with capability restrictions
        if (LLVMModules_useWith(gen, capabilities, capCount, modulePath, node->lineNumber) != 0) {
          fprintf(stderr, "ERROR: Failed to load module '%s' with capabilities at line %d\n",
                  modulePath, node->lineNumber);
          free(capabilities);
          return LLVMConstInt(gen->intType, 0, 0);
        }
      }

      free(capabilities);

      // Return void
      return LLVMConstInt(gen->intType, 0, 0);
    }

    //  Check if it's a closure (stored as variable, not function)
    // INDUSTRY STANDARD: Use metadata tracking instead of type introspection
    // (LLVM 17 opaque pointers don't support LLVMGetElementType)
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "\n[CLOSURE LOOKUP DEBUG] ========== Looking for '%s' as closure ==========\n", funcName);
      #endif
    }
    LLVMValueRef closureVar = LLVMVariableMap_get(gen->variables, funcName);
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[CLOSURE LOOKUP DEBUG] closureVar from variables map = %p\n", closureVar);
      #endif
    }

    if (closureVar) {
      // Check if this variable is marked as a closure in our closure map
      //  MAP2: Allow function parameters to be called as closures
      // If a variable exists and is being called like (variable arg1 arg2),
      // treat it as a potential closure even if not in closures map
      int isClosure = LLVMVariableMap_get(gen->closures, funcName) != NULL;
      int isParameter = 1; // Assume it might be a function parameter

      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[CLOSURE LOOKUP DEBUG] isClosure (from closures map) = %d\n", isClosure);
        #endif
        #if 0  // Debug output disabled
        fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Treating as callable (closure or parameter)\n");
        #endif
      }

      // Proceed if it's either a known closure OR a potential function parameter
      if (isClosure || isParameter) {
        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Confirmed as callable, preparing to call\n");
          #endif
        }
        // It's a closure (or function parameter) - load it if needed
        LLVMValueRef actualValue = closureVar;

        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[CLOSURE LOOKUP DEBUG] closureVar type kind = %d\n", LLVMGetTypeKind(LLVMTypeOf(closureVar)));
          #endif
          #if 0  // Debug output disabled
          fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Is alloca? %d\n", LLVMIsAAllocaInst(closureVar) != NULL);
          #endif
        }

        if (LLVMIsAAllocaInst(closureVar)) {
          // It's an alloca - get the allocated type and load
          LLVMTypeRef allocatedType = LLVMGetAllocatedType(closureVar);
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Allocated type kind = %d\n", LLVMGetTypeKind(allocatedType));
            #endif
          }
          actualValue = LLVMBuildLoad2(gen->builder, allocatedType, closureVar, "loaded_closure");
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Loaded value type kind = %d\n", LLVMGetTypeKind(LLVMTypeOf(actualValue)));
            #endif
          }
        }

        //  FIX: Check if this is a Generic*-wrapped closure (nested closure case)
        // For factory patterns, closures can return other closures wrapped in Generic*
        // We need to unbox the Generic* to get the raw closure i64
        int isGenericPtr = LLVMVariableMap_get(gen->genericVariables, funcName) != NULL;
        if (isGenericPtr) {
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Variable is Generic*, unboxing to get closure\n");
            #endif
          }
          // Declare runtime function: int64_t franz_generic_to_closure_ptr(int64_t)
          LLVMValueRef unboxFunc = LLVMGetNamedFunction(gen->module, "franz_generic_to_closure_ptr");
          if (!unboxFunc) {
            LLVMTypeRef unboxParams[] = {gen->intType};
            LLVMTypeRef unboxType = LLVMFunctionType(gen->intType, unboxParams, 1, 0);
            unboxFunc = LLVMAddFunction(gen->module, "franz_generic_to_closure_ptr", unboxType);
            if (gen->debugMode) {
              #if 0  // Debug output disabled
              fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Declared franz_generic_to_closure_ptr function\n");
              #endif
            }
          }

          // Call unbox function
          LLVMValueRef unboxArgs[1] = { actualValue };
          actualValue = LLVMBuildCall2(gen->builder,
                                       LLVMFunctionType(gen->intType, (LLVMTypeRef[]){gen->intType}, 1, 0),
                                       unboxFunc,
                                       unboxArgs, 1,
                                       "unboxed_closure");
          if (gen->debugMode) {
            #if 0  // Debug output disabled
            fprintf(stderr, "[CLOSURE LOOKUP DEBUG] Unboxed Generic* to closure i64\n");
            #endif
          }
        }

        // Compile arguments and call as closure
        LLVMValueRef *args = malloc(argNode.childCount * sizeof(LLVMValueRef));
        #if 0  // Debug output disabled
        fprintf(stderr, "[CLOSURE CALL DEBUG] Compiling %d arguments\n", argNode.childCount);
        #endif
        for (int i = 0; i < argNode.childCount; i++) {
          args[i] = LLVMCodeGen_compileNode_impl(gen, argNode.children[i]);
          if (!args[i]) {
            free(args);
            return NULL;
          }
          #if 0  // Debug output disabled
          fprintf(stderr, "[CLOSURE CALL DEBUG] arg[%d] compiled successfully\n", i);
          #endif
        }

        #if 0  // Debug output disabled
        fprintf(stderr, "[CLOSURE CALL DEBUG] Calling LLVMClosures_callClosure with %d args\n", argNode.childCount);
        #endif
        LLVMValueRef result = LLVMClosures_callClosure(gen, actualValue, args, argNode.childCount, argNode.children);
        #if 0  // Debug output disabled
        fprintf(stderr, "[CLOSURE CALL DEBUG] Closure call returned: %p\n", (void*)result);
        #endif
        free(args);
        return result;
      }
    }

    //  Check for user-defined functions in functions map
    LLVMValueRef userFunc = LLVMVariableMap_get(gen->functions, funcName);
    if (userFunc) {
      // CRITICAL DEBUG: Trace function call type resolution
      #if 0  // Debug output disabled
      fprintf(stderr, "\n[FUNCTION CALL DEBUG] ========== Calling '%s' ==========\n", funcName);
      #endif
      #if 0  // Debug output disabled
      fprintf(stderr, "[FUNCTION CALL DEBUG] userFunc = %p\n", userFunc);
      #endif
      #if 0  // Debug output disabled
      fprintf(stderr, "[FUNCTION CALL DEBUG] userFunc type kind = %d\n", LLVMGetTypeKind(LLVMTypeOf(userFunc)));
      #endif

      const char *userFuncName = LLVMGetValueName(userFunc);
      #if 0  // Debug output disabled
      fprintf(stderr, "[FUNCTION CALL DEBUG] userFunc value name = '%s'\n", userFuncName ? userFuncName : "(null)");
      #endif

      // Call user-defined function
      // userFunc is already an LLVMValueRef function, use it directly

      // Get the function type
      LLVMTypeRef funcType = LLVMGlobalGetValueType(userFunc);
      #if 0  // Debug output disabled
      fprintf(stderr, "[FUNCTION CALL DEBUG] funcType = %p\n", funcType);
      #endif
      #if 0  // Debug output disabled
      fprintf(stderr, "[FUNCTION CALL DEBUG] funcType kind = %d\n", LLVMGetTypeKind(funcType));
      #endif

      // Get parameter types for type conversion
      unsigned int paramCount = LLVMCountParamTypes(funcType);
      LLVMTypeRef *paramTypes = NULL;
      if (paramCount > 0) {
        paramTypes = malloc(paramCount * sizeof(LLVMTypeRef));
        LLVMGetParamTypes(funcType, paramTypes);
      }

      // Compile arguments
      LLVMValueRef *args = malloc(argNode.childCount * sizeof(LLVMValueRef));
      for (int i = 0; i < argNode.childCount; i++) {
        args[i] = LLVMCodeGen_compileNode_impl(gen, argNode.children[i]);
        if (!args[i]) {
          free(args);
          if (paramTypes) free(paramTypes);
          return NULL;
        }

        //  Convert argument to match expected parameter type
        if (paramTypes && i < (int)paramCount) {
          LLVMTypeRef expectedType = paramTypes[i];
          LLVMTypeRef actualType = LLVMTypeOf(args[i]);

          // Convert if types don't match
          if (actualType != expectedType) {
            if (actualType == gen->intType && expectedType == gen->floatType) {
              // int → float conversion
              args[i] = LLVMBuildSIToFP(gen->builder, args[i], gen->floatType, "itof");
            } else if (actualType == gen->floatType && expectedType == gen->intType) {
              // float → int conversion
              args[i] = LLVMBuildFPToSI(gen->builder, args[i], gen->intType, "ftoi");
            }
            // Other conversions can be added here if needed
          }
        }
      }

      LLVMValueRef result = LLVMBuildCall2(gen->builder, funcType, userFunc, args, argNode.childCount, "call");

      // TCO: Mark as tail call if in tail position
      if (gen->enableTCO && gen->inTailPosition) {
        LLVMSetTailCall(result, 1);  // Mark as tail call for LLVM optimization

        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[TCO] Marked tail call: %s at line %d\n", funcName, node->lineNumber);
          #endif
        }
      }

      free(args);
      if (paramTypes) free(paramTypes);
      return result;
    }

    // Dispatch to stdlib function handlers
    if (strcmp(funcName, "add") == 0) {
      return LLVMCodeGen_compileAdd_impl(gen, &argNode);
    } else if (strcmp(funcName, "subtract") == 0) {
      return LLVMCodeGen_compileSubtract_impl(gen, &argNode);
    } else if (strcmp(funcName, "multiply") == 0) {
      return LLVMCodeGen_compileMultiply_impl(gen, &argNode);
    } else if (strcmp(funcName, "divide") == 0) {
      return LLVMCodeGen_compileDivide_impl(gen, &argNode);
    } else if (strcmp(funcName, "println") == 0) {
      return LLVMCodeGen_compilePrintln_impl(gen, &argNode);
    } else if (strcmp(funcName, "print") == 0) {
      return LLVMCodeGen_compilePrint_impl(gen, &argNode);
    } else if (strcmp(funcName, "input") == 0) {
      return LLVMCodeGen_compileInput_impl(gen, &argNode);
    } else if (strcmp(funcName, "rows") == 0) {
      return LLVMCodeGen_compileRows_impl(gen, &argNode);
    } else if (strcmp(funcName, "columns") == 0) {
      return LLVMCodeGen_compileColumns_impl(gen, &argNode);
    } else if (strcmp(funcName, "repeat") == 0) {
      return LLVMCodeGen_compileRepeat_impl(gen, &argNode);
    } else if (strcmp(funcName, "read_file") == 0) {
      //  Read file contents as string
      return LLVMFileOps_compileReadFile(gen, &argNode);
    } else if (strcmp(funcName, "write_file") == 0) {
      //  Write string contents to file
      return LLVMFileOps_compileWriteFile(gen, &argNode);
    } else if (strcmp(funcName, "file_exists") == 0) {
      //  Check if file exists
      return LLVMFileOps_compileFileExists(gen, &argNode);
    } else if (strcmp(funcName, "append_file") == 0) {
      //  Append contents to file
      return LLVMFileOps_compileAppendFile(gen, &argNode);
    } else if (strcmp(funcName, "read_binary") == 0) {
      //  Read binary file
      return LLVMFileAdvanced_compileReadBinary(gen, &argNode);
    } else if (strcmp(funcName, "write_binary") == 0) {
      //  Write binary file
      return LLVMFileAdvanced_compileWriteBinary(gen, &argNode);
    } else if (strcmp(funcName, "list_files") == 0) {
      //  List files in directory (returns Generic* via franz_list_files helper)
      return LLVMFileAdvanced_compileListFiles(gen, &argNode);
    } else if (strcmp(funcName, "create_dir") == 0) {
      //  Create directory
      return LLVMFileAdvanced_compileCreateDir(gen, &argNode);
    } else if (strcmp(funcName, "dir_exists") == 0) {
      //  Check if directory exists
      return LLVMFileAdvanced_compileDirExists(gen, &argNode);
    } else if (strcmp(funcName, "remove_dir") == 0) {
      //  Remove directory
      return LLVMFileAdvanced_compileRemoveDir(gen, &argNode);
    } else if (strcmp(funcName, "file_size") == 0) {
      //  Get file size
      return LLVMFileAdvanced_compileFileSize(gen, &argNode);
    } else if (strcmp(funcName, "file_mtime") == 0) {
      //  Get file modification time
      return LLVMFileAdvanced_compileFileMtime(gen, &argNode);
    } else if (strcmp(funcName, "is_directory") == 0) {
      //  Check if path is directory
      return LLVMFileAdvanced_compileIsDirectory(gen, &argNode);
    } else if (strcmp(funcName, "integer") == 0) {
      return LLVMCodeGen_compileInteger_func_impl(gen, &argNode);
    } else if (strcmp(funcName, "float") == 0) {
      return LLVMCodeGen_compileFloat_func_impl(gen, &argNode);
    } else if (strcmp(funcName, "string") == 0) {
      return LLVMCodeGen_compileString_func_impl(gen, &argNode);
    } else if (strcmp(funcName, "format-int") == 0) {
      return LLVMCodeGen_compileFormatInt_impl(gen, &argNode);
    } else if (strcmp(funcName, "format-float") == 0) {
      return LLVMCodeGen_compileFormatFloat_impl(gen, &argNode);
    } else if (strcmp(funcName, "join") == 0) {
      return LLVMCodeGen_compileJoin_impl(gen, &argNode);
    } else if (strcmp(funcName, "remainder") == 0) {
      return LLVMCodeGen_compileRemainder(gen, &argNode);
    } else if (strcmp(funcName, "power") == 0) {
      return LLVMCodeGen_compilePower(gen, &argNode);
    } else if (strcmp(funcName, "random") == 0) {
      return LLVMCodeGen_compileRandom(gen, &argNode);
    } else if (strcmp(funcName, "random_int") == 0) {
      return LLVMCodeGen_compileRandomInt(gen, &argNode);
    } else if (strcmp(funcName, "random_range") == 0) {
      return LLVMCodeGen_compileRandomRange(gen, &argNode);
    } else if (strcmp(funcName, "random_seed") == 0) {
      return LLVMCodeGen_compileRandomSeed(gen, &argNode);
    } else if (strcmp(funcName, "floor") == 0) {
      return LLVMCodeGen_compileFloor(gen, &argNode);
    } else if (strcmp(funcName, "ceil") == 0) {
      return LLVMCodeGen_compileCeil(gen, &argNode);
    } else if (strcmp(funcName, "round") == 0) {
      return LLVMCodeGen_compileRound(gen, &argNode);
    } else if (strcmp(funcName, "abs") == 0) {
      return LLVMCodeGen_compileAbs(gen, &argNode);
    } else if (strcmp(funcName, "min") == 0) {
      return LLVMCodeGen_compileMin(gen, &argNode);
    } else if (strcmp(funcName, "max") == 0) {
      return LLVMCodeGen_compileMax(gen, &argNode);
    } else if (strcmp(funcName, "sqrt") == 0) {
      return LLVMCodeGen_compileSqrt(gen, &argNode);
    } else if (strcmp(funcName, "is") == 0) {
      return LLVMCodeGen_compileIs(gen, &argNode);
    } else if (strcmp(funcName, "less_than") == 0) {
      return LLVMCodeGen_compileLessThan(gen, &argNode);
    } else if (strcmp(funcName, "greater_than") == 0) {
      return LLVMCodeGen_compileGreaterThan(gen, &argNode);
    } else if (strcmp(funcName, "not") == 0) {
      return LLVMCodeGen_compileNot(gen, &argNode);
    } else if (strcmp(funcName, "and") == 0) {
      //  Use short-circuit evaluation (industry standard)
      return LLVMCodeGen_compileAndShortCircuit(gen, &argNode);
    } else if (strcmp(funcName, "or") == 0) {
      //  Use short-circuit evaluation (industry standard)
      return LLVMCodeGen_compileOrShortCircuit(gen, &argNode);
    } else if (strcmp(funcName, "if") == 0) {
      return LLVMCodeGen_compileIf(gen, &argNode);
    } else if (strcmp(funcName, "when") == 0) {
      //  when helper - execute action if condition true
      return LLVMCodeGen_compileWhen(gen, &argNode);
    } else if (strcmp(funcName, "unless") == 0) {
      //  unless helper - execute action if condition false
      return LLVMCodeGen_compileUnless(gen, &argNode);
    } else if (strcmp(funcName, "is_int") == 0) {
      //  type guard - check if value is integer
      return LLVMCodeGen_compileIsInt(gen, &argNode);
    } else if (strcmp(funcName, "is_float") == 0) {
      //  type guard - check if value is float
      return LLVMCodeGen_compileIsFloat(gen, &argNode);
    } else if (strcmp(funcName, "is_string") == 0) {
      //  type guard - check if value is string
      return LLVMCodeGen_compileIsString(gen, &argNode);
    } else if (strcmp(funcName, "is_list") == 0) {
      //  type guard - check if value is list
      return LLVMCodeGen_compileIsList(gen, &argNode);
    } else if (strcmp(funcName, "is_function") == 0) {
      //  type guard - check if value is function (TODO)
      return LLVMCodeGen_compileIsFunction(gen, &argNode);
    } else if (strcmp(funcName, "type") == 0) {
      //  Runtime type introspection - returns type name as string
      return LLVMType_compileType(gen, &argNode);
    } else if (strcmp(funcName, "head") == 0) {
      //  get first element of list
      return LLVMListOps_compileHead(gen, &argNode);
    } else if (strcmp(funcName, "tail") == 0) {
      //  get rest of list
      return LLVMListOps_compileTail(gen, &argNode);
    } else if (strcmp(funcName, "cons") == 0) {
      //  prepend element to list
      return LLVMListOps_compileCons(gen, &argNode);
    } else if (strcmp(funcName, "empty?") == 0) {
      //  check if list is empty
      return LLVMListOps_compileIsEmpty(gen, &argNode);
    } else if (strcmp(funcName, "length") == 0) {
      //  get list length
      return LLVMListOps_compileLength(gen, &argNode);
    } else if (strcmp(funcName, "nth") == 0) {
      //  get element at index
      return LLVMListOps_compileNth(gen, &argNode);
    } else if (strcmp(funcName, "filter") == 0) {
      // LLVM Filter: filter list with predicate closure
      return LLVMFilter_compileFilter(gen, &argNode);
    } else if (strcmp(funcName, "map") == 0) {
      // LLVM Map: transform list with callback closure
      return LLVMMap_compileMap(gen, &argNode);
    } else if (strcmp(funcName, "map2") == 0) {
      // LLVM Map2: transform two lists with callback closure (zip-map)
      return LLVMMap_compileMap2(gen, &argNode);
    } else if (strcmp(funcName, "reduce") == 0) {
      // LLVM Reduce: reduce list with callback closure and initial value
      return LLVMReduce_compileReduce(gen, &argNode);
    } else if (strcmp(funcName, "ref") == 0) {
      //  Create mutable reference
      return LLVMCodeGen_compileRef(gen, &argNode);
    } else if (strcmp(funcName, "deref") == 0) {
      //  Dereference (read value)
      return LLVMCodeGen_compileDeref(gen, &argNode);
    } else if (strcmp(funcName, "set!") == 0) {
      //  Update mutable reference
      return LLVMCodeGen_compileSetRef(gen, &argNode);
    } else if (strcmp(funcName, "get") == 0) {
      //  get substring or list element
      return LLVMCodeGen_compileGet_impl(gen, &argNode);
    } else if (strcmp(funcName, "variant") == 0) {
      //  ADT variant construction
      return LLVMAdt_compileVariant(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "match") == 0) {
      //  ADT pattern matching
      return LLVMAdt_compileMatch(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict") == 0) {
      // Dict: dictionary/hash map creation
      return LLVMDict_compileDict(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_get") == 0) {
      // Dict: get value by key
      return LLVMDict_compileDictGet(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_set") == 0) {
      // Dict: immutable update
      return LLVMDict_compileDictSet(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_has") == 0) {
      // Dict: check key existence
      return LLVMDict_compileDictHas(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_keys") == 0) {
      // Dict: get all keys
      return LLVMDict_compileDictKeys(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_values") == 0) {
      // Dict: get all values
      return LLVMDict_compileDictValues(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_merge") == 0) {
      // Dict: merge two dicts
      return LLVMDict_compileDictMerge(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_map") == 0) {
      // Dict: map over values with closure
      return LLVMDict_compileDictMap(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "dict_filter") == 0) {
      // Dict: filter entries with closure
      return LLVMDict_compileDictFilter(gen, node, node->lineNumber);
    } else if (strcmp(funcName, "cond") == 0) {
      //  cond chains - pattern matching conditionals
      return LLVMCodeGen_compileCond(gen, &argNode);
    } else if (strcmp(funcName, "loop") == 0) {
      //  loop - counted iteration
      return LLVMCodeGen_compileLoop(gen, &argNode);
    } else if (strcmp(funcName, "break") == 0) {
      //  break - early loop exit
      if (gen->loopExitBlock == NULL) {
        fprintf(stderr, "ERROR: 'break' can only be used inside a loop at line %d\n", node->lineNumber);
        return NULL;
      }

      LLVMValueRef breakValue = LLVMConstInt(gen->intType, 0, 0);  // Default: break with 0
      if (argNode.childCount > 0) {
        // break with value
        breakValue = LLVMCodeGen_compileNode_impl(gen, argNode.children[0]);
        if (!breakValue) {
          fprintf(stderr, "ERROR: Failed to compile break value at line %d\n", node->lineNumber);
          return NULL;
        }
      }

      // Store break value (convert to pointer for generic storage)
      if (gen->loopReturnPtr) {
        LLVMTypeRef breakType = LLVMTypeOf(breakValue);
        LLVMValueRef breakPtr;
        
        // Update loop return type tracker
        gen->loopReturnType = breakType;
        
        // Convert value to pointer type for storage
        if (breakType == gen->intType) {
          breakPtr = LLVMBuildIntToPtr(gen->builder, breakValue, gen->stringType, "break_as_ptr");
        } else if (breakType == gen->floatType) {
          // Float -> int -> pointer
          LLVMValueRef asInt = LLVMBuildFPToSI(gen->builder, breakValue, gen->intType, "float_as_int");
          breakPtr = LLVMBuildIntToPtr(gen->builder, asInt, gen->stringType, "break_as_ptr");
        } else {
          // Already a pointer (string, etc.)
          breakPtr = breakValue;
        }
        
        LLVMBuildStore(gen->builder, breakPtr, gen->loopReturnPtr);
      }

      // Branch to loop exit
      LLVMBuildBr(gen->builder, gen->loopExitBlock);
      return breakValue;
    } else if (strcmp(funcName, "continue") == 0) {
      //  continue - skip to next iteration
      if (gen->loopIncrBlock == NULL) {
        fprintf(stderr, "ERROR: 'continue' can only be used inside a loop at line %d\n", node->lineNumber);
        return NULL;
      }

      // Branch to loop increment block (skip rest of iteration)
      LLVMBuildBr(gen->builder, gen->loopIncrBlock);

      // Return 0 to satisfy the type system (value won't be used)
      return LLVMConstInt(gen->intType, 0, 0);
    } else if (strcmp(funcName, "while") == 0) {
      //  while - condition-based iteration
      return LLVMCodeGen_compileWhile(gen, &argNode);
    } else {
      fprintf(stderr, "ERROR: Unknown function '%s' at line %d\n",
              funcName, node->lineNumber);
      fprintf(stderr, " supports: add, subtract, multiply, divide, println, print, input, rows, columns, repeat, read_file, write_file, integer, float, string, format-int, format-float, join, remainder, power, random, random_int, random_range, random_seed, floor, ceil, round, abs, min, max, sqrt, is, less_than, greater_than, not, and, or, if, when, unless, is_int, is_float, is_string, is_list, is_function, type, cond, loop, while, break, continue, and user-defined functions\n");
      return NULL;
    }
  } else {
    // Higher-order function call (function is an expression)
    fprintf(stderr, "ERROR: Higher-order function calls not yet supported at line %d\n", node->lineNumber);
    return NULL;
  }
}

// ============================================================================
// Statement Node Handler
// ============================================================================

LLVMValueRef LLVMCodeGen_compileStatement_impl(LLVMCodeGen *gen, AstNode *node) {
  LLVMValueRef lastValue = NULL;

  // Execute each child statement
  for (int i = 0; i < node->childCount; i++) {
    lastValue = LLVMCodeGen_compileNode_impl(gen, node->children[i]);
  }

  return lastValue;
}

// ============================================================================
//  Wrapper Function Generator (Rust/Clang Style)
// ============================================================================

// INDUSTRY-STANDARD: Generate wrapper function that adapts regular function ABI to closure ABI
// Example: i64 func(i64 x) → i8* wrapper(i8* env, i64 x)
//
// This matches how Rust converts `fn` items to `dyn Fn` trait objects
// and how Clang handles function pointer conversions
static LLVMValueRef createClosureWrapper(
    LLVMCodeGen *gen,
    LLVMValueRef originalFunc,  // The function to wrap
    int paramCount,              // Number of parameters
    LLVMTypeRef *paramTypes,     // Parameter types
    LLVMTypeRef returnType       // Return type
) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[WRAPPER] Creating wrapper for function with %d params\n", paramCount);
    #endif

    // Save current builder position
    LLVMBasicBlockRef savedBlock = LLVMGetInsertBlock(gen->builder);

    // Step 1: Create wrapper function type
    // Wrapper accepts: (env*, arg1, arg2, ...) and returns i8*
    // CRITICAL FIX: Wrapper parameters are ALL i64 (Franz's universal type)
    // We'll convert them back to original types when calling the original function
    LLVMTypeRef *wrapperParamTypes = malloc((paramCount + 1) * sizeof(LLVMTypeRef));
    wrapperParamTypes[0] = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);  // env*
    for (int i = 0; i < paramCount; i++) {
        wrapperParamTypes[i + 1] = gen->intType;  // All params are i64 in Franz
    }

    // INDUSTRY-STANDARD FIX: Use universal i8* return (like Rust's trait objects / OCaml's universal type)
    // All wrappers return i8* to maintain uniform closure calling convention
    // This allows closures of different types to be stored in the same struct
    // We convert back to the original type based on the runtime type tag
    LLVMTypeRef wrapperReturn = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);  // i8*
    LLVMTypeRef wrapperType = LLVMFunctionType(wrapperReturn, wrapperParamTypes, paramCount + 1, 0);

    // Step 2: Create wrapper function
    static int wrapperCounter = 0;
    char wrapperName[64];
    snprintf(wrapperName, sizeof(wrapperName), "_franz_wrapper_%d", wrapperCounter++);
    LLVMValueRef wrapper = LLVMAddFunction(gen->module, wrapperName, wrapperType);

    // Step 3: Build wrapper body
    LLVMBasicBlockRef wrapperEntry = LLVMAppendBasicBlockInContext(gen->context, wrapper, "entry");
    LLVMPositionBuilderAtEnd(gen->builder, wrapperEntry);

    // Step 4: Extract arguments (skip env* at index 0)
    // CRITICAL FIX: Wrapper receives i64 parameters, but original function may expect different types
    // Convert i64 back to original types before calling
    LLVMValueRef *callArgs = malloc(paramCount * sizeof(LLVMValueRef));
    for (int i = 0; i < paramCount; i++) {
        LLVMValueRef i64Param = LLVMGetParam(wrapper, i + 1);  // Get i64 parameter
        LLVMTypeKind origKind = LLVMGetTypeKind(paramTypes[i]);

        if (origKind == LLVMPointerTypeKind) {
            // Original expects pointer - convert i64 to pointer
            callArgs[i] = LLVMBuildIntToPtr(gen->builder, i64Param, paramTypes[i], "i64_to_ptr");
        } else if (origKind == LLVMIntegerTypeKind) {
            // Original expects i64 - use as-is
            callArgs[i] = i64Param;
        } else if (origKind == LLVMDoubleTypeKind || origKind == LLVMFloatTypeKind) {
            // Original expects float - bitcast i64 to double
            callArgs[i] = LLVMBuildBitCast(gen->builder, i64Param, paramTypes[i], "i64_to_float");
        } else {
            // Unknown - use as-is
            callArgs[i] = i64Param;
        }
    }

    // Step 5: Call original function
    LLVMTypeRef origFuncType = LLVMFunctionType(returnType, paramTypes, paramCount, 0);
    LLVMValueRef result = LLVMBuildCall2(gen->builder, origFuncType, originalFunc, callArgs, paramCount, "call_original");

    // Step 6: Convert return value to universal i8* based on type
    // This is the CRITICAL conversion that preserves the original value semantics
    LLVMValueRef universalResult;
    LLVMTypeKind resultKind = LLVMGetTypeKind(returnType);

    if (resultKind == LLVMPointerTypeKind) {
        // Already a pointer (string, list, etc.) - cast to i8*
        universalResult = LLVMBuildPointerCast(gen->builder, result, wrapperReturn, "ptr_to_i8ptr");
    } else if (resultKind == LLVMIntegerTypeKind) {
        // Integer (i64) - convert to i8* via inttoptr
        // CRITICAL: For raw integers (like 42), this creates an invalid pointer address
        // But it preserves the integer value, which we reverse with ptrtoint later
        universalResult = LLVMBuildIntToPtr(gen->builder, result, wrapperReturn, "int_to_i8ptr");
    } else if (resultKind == LLVMDoubleTypeKind || resultKind == LLVMFloatTypeKind) {
        // Float - bitcast to i64 first, then inttoptr
        LLVMValueRef asInt = LLVMBuildBitCast(gen->builder, result, gen->intType, "float_to_int");
        universalResult = LLVMBuildIntToPtr(gen->builder, asInt, wrapperReturn, "float_to_i8ptr");
    } else {
        // Unknown type - try pointer cast
        universalResult = LLVMBuildPointerCast(gen->builder, result, wrapperReturn, "unknown_to_i8ptr");
    }

    LLVMBuildRet(gen->builder, universalResult);

    free(callArgs);
    free(wrapperParamTypes);

    // Restore builder position
    if (savedBlock) {
        LLVMPositionBuilderAtEnd(gen->builder, savedBlock);
    }

    #if 0  // Debug output disabled
    fprintf(stderr, "[WRAPPER] Wrapper created successfully: %s\n", wrapperName);
    #endif
    return wrapper;
}

// ============================================================================
//  Function Definition Handler
// ============================================================================

LLVMValueRef LLVMCodeGen_compileFunction_impl(LLVMCodeGen *gen, AstNode *node) {
  //  Functions with type inference for float/string support
  //  Closure support - detect and handle closures
  // Function node structure:
  //   - children[0..n-1]: Parameter identifiers
  //   - children[n]: Function body (statement or return)

  if (node->childCount == 0) {
    fprintf(stderr, "ERROR: Empty function definition at line %d\n", node->lineNumber);
    return NULL;
  }

  //  TCO: Check if forward declaration exists FIRST
  // If we have a forward declaration (for recursion), DON'T compile as closure
  // even if function has free variables (self-recursion case)
  int hasForwardDecl = 0;
  
  // CRITICAL FIX: gen->currentFunction is set to "main" for top-level statements
  // So we check if we're inside "main" and treat that as top-level context
  int isInsideMain = 0;
  if (gen->currentFunction != NULL) {
    const char *curFuncName = LLVMGetValueName(gen->currentFunction);
    isInsideMain = (curFuncName && strcmp(curFuncName, "main") == 0);
  }
  
  // Check for forward declarations at top-level (NULL or inside main)
  if ((gen->currentFunction == NULL || isInsideMain) && gen->currentFunctionName != NULL) {
    LLVMValueRef forwardDecl = LLVMVariableMap_get(gen->functions, gen->currentFunctionName);
    if (forwardDecl) {
      hasForwardDecl = 1;
    }
  }

  //  Nested functions are compiled as closures by default.
  // This ensures capture works even when free var analysis is conservative.
  // Skip if we have a forward declaration OR if we're inside main (top-level functions are NOT closures)
  if (gen->currentFunction != NULL && !hasForwardDecl && !isInsideMain) {
    return LLVMClosures_compileClosure(gen, node);
  }

  // B: Properly count parameters (OP_IDENTIFIER nodes)
  // In multi-statement bodies, children are: [param1, param2, ..., stmt1, stmt2, ...]
  // Parameters are ALWAYS OP_IDENTIFIER, body statements are OP_STATEMENT/OP_RETURN
  int paramCount = 0;
  while (paramCount < node->childCount &&
         node->children[paramCount]->opcode == OP_IDENTIFIER) {
    paramCount++;
  }

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Function has %d parameters, %d total children\n",
            paramCount, node->childCount);
    #endif
  }

  // Validate we have at least one body statement
  if (paramCount >= node->childCount) {
    fprintf(stderr, "ERROR: Function has no body at line %d\n", node->lineNumber);
    return NULL;
  }

  // B: Check if single-statement or multi-statement body
  int bodyStmtCount = node->childCount - paramCount;
  int isMultiStatementBody = (bodyStmtCount > 1);

  //  Infer function type signature
  InferredFunctionType *inferredType = TypeInfer_inferFunction(node);
  if (!inferredType) {
    fprintf(stderr, "ERROR: Failed to infer function type at line %d\n", node->lineNumber);
    return NULL;
  }

  // Build parameter types from inference (needed for type checking later)
  LLVMTypeRef *paramTypes = malloc(paramCount * sizeof(LLVMTypeRef));
  for (int i = 0; i < paramCount; i++) {
    paramTypes[i] = TypeInfer_getLLVMType(inferredType->paramTypes[i], gen->context);
  }

  // Get inferred return type (needed for type conversions later)
  LLVMTypeRef returnType = TypeInfer_getLLVMType(inferredType->returnType, gen->context);

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Initial returnType: kind=%d, inferred=%d\n",
            (int)LLVMGetTypeKind(returnType), inferredType->returnType);
    #endif
  }

  //  If function body is a closure, override return type to pointer
  // Type inference doesn't handle closures, so we detect factory functions here
  // Example: {n -> <- {x -> ...}} returns a closure pointer, not i64

  // B: For multi-statement bodies, check the LAST statement for closure return
  AstNode *lastBodyStmt = node->children[node->childCount - 1];
  AstNode *actualBody = lastBodyStmt;

  // Unwrap STATEMENT nodes - body might be wrapped
  if (lastBodyStmt && lastBodyStmt->opcode == OP_STATEMENT && lastBodyStmt->childCount > 0) {
    actualBody = lastBodyStmt->children[lastBodyStmt->childCount - 1];
  }

  if (actualBody && actualBody->opcode == OP_FUNCTION) {
    // Body is a function/closure - return type should be pointer to closure struct
    returnType = LLVMClosures_getClosureType(gen->context);
    returnType = LLVMPointerType(returnType, 0);  // Pointer to closure struct
  } else if (actualBody && actualBody->opcode == OP_RETURN && actualBody->childCount > 0) {
    // Check if return statement returns a closure: <- {x -> ...}
    AstNode *returnValue = actualBody->children[0];
    if (returnValue && returnValue->opcode == OP_FUNCTION) {
      returnType = LLVMClosures_getClosureType(gen->context);
      returnType = LLVMPointerType(returnType, 0);  // Pointer to closure struct
    }
  }

  //  Check if this function already has a forward declaration
  // (from Pass 1 for mutual recursion support)
  LLVMValueRef function = NULL;
  if (gen->currentFunctionName != NULL) {
    function = LLVMVariableMap_get(gen->functions, gen->currentFunctionName);
    if (function && gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Using forward declaration for '%s'\n", gen->currentFunctionName);
      #endif
    }
  }

  //  Track if we need to fix return type after body compilation
  int needsReturnTypeFix = (inferredType->returnType == INFER_TYPE_UNKNOWN);
  char tempFuncName[64] = {0};

  if (gen->debugMode && needsReturnTypeFix) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Function has UNKNOWN return type - will fix after body compilation\n");
    #endif
  }

  // If no forward declaration exists, create a new function
  if (!function) {
    // Generate unique function name
    static int funcCounter = 0;
    snprintf(tempFuncName, sizeof(tempFuncName), "_franz_lambda_%d", funcCounter++);

    // Create function type with inferred return type
    LLVMTypeRef funcType = LLVMFunctionType(returnType, paramTypes, paramCount, 0);
    function = LLVMAddFunction(gen->module, tempFuncName, funcType);

    // Register function in symbol table (for self-recursion)
    if (gen->currentFunctionName != NULL) {
      LLVMVariableMap_set(gen->functions, gen->currentFunctionName, function);

      //  Store inferred return type tag for upstream tagging optimization
      // Convert InferredTypeKind → ClosureReturnTypeTag and store in map
      int returnTypeTag = TypeInfer_toReturnTypeTag(inferredType->returnType);
      LLVMVariableMap_set(gen->returnTypeTags, gen->currentFunctionName,
                          (LLVMValueRef)(intptr_t)(returnTypeTag + 1));  // +1 to avoid NULL

      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Registered function '%s' for recursion (return tag: %d)\n",
                gen->currentFunctionName, returnTypeTag);
        #endif
      }
    }
  } else {
    // Using forward declaration - get its name
    const char *existingName = LLVMGetValueName(function);
    if (existingName) {
      snprintf(tempFuncName, sizeof(tempFuncName), "%s", existingName);
    }
  }

  // Save current function and builder position
  LLVMValueRef prevFunction = gen->currentFunction;
  LLVMBasicBlockRef prevBlock = LLVMGetInsertBlock(gen->builder);

  //  Save and set polymorphic function flag
  int prevIsPolymorphic = gen->isPolymorphicFunction;
  gen->isPolymorphicFunction = needsReturnTypeFix;

  // Create entry block for the new function
  LLVMBasicBlockRef entryBlock = LLVMAppendBasicBlockInContext(gen->context, function, "entry");
  LLVMPositionBuilderAtEnd(gen->builder, entryBlock);

  // Set as current function
  gen->currentFunction = function;

  // Create a new variable map for function scope (save old one)
  LLVMVariableMap *prevVars = gen->variables;
  gen->variables = LLVMVariableMap_new();

  // Bind parameters to their values
  for (int i = 0; i < paramCount; i++) {
    AstNode *paramNode = node->children[i];
    if (paramNode->opcode != OP_IDENTIFIER) {
      fprintf(stderr, "ERROR: Function parameter must be identifier at line %d\n", paramNode->lineNumber);
      free(paramTypes);
      TypeInfer_freeInferredType(inferredType);  //  Cleanup
      LLVMVariableMap_free(gen->variables);
      gen->variables = prevVars;
      gen->currentFunction = prevFunction;
      return NULL;
    }

    LLVMValueRef param = LLVMGetParam(function, i);
    LLVMVariableMap_set(gen->variables, paramNode->val, param);
  }

  // B: Compile function body (single or multi-statement)
  LLVMValueRef bodyValue = NULL;

  if (isMultiStatementBody) {
    // Multi-statement body: compile each statement in sequence
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Compiling multi-statement body with %d statements\n", bodyStmtCount);
      #endif
    }

    for (int i = paramCount; i < node->childCount; i++) {
      AstNode *stmt = node->children[i];
      LLVMValueRef stmtValue = LLVMCodeGen_compileNode_impl(gen, stmt);

      if (!stmtValue) {
        fprintf(stderr, "ERROR: Failed to compile statement %d in function body at line %d\n",
                i - paramCount + 1, stmt->lineNumber);
        free(paramTypes);
        TypeInfer_freeInferredType(inferredType);
        LLVMVariableMap_free(gen->variables);
        gen->variables = prevVars;
        gen->currentFunction = prevFunction;
        return NULL;
      }

      // Keep the last statement's value as the function return value
      bodyValue = stmtValue;
    }
  } else {
    // Single-statement body (original behavior)
    AstNode *singleBodyStmt = node->children[paramCount];

    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Compiling single-statement body\n");
      #endif
    }

    bodyValue = LLVMCodeGen_compileNode_impl(gen, singleBodyStmt);

    if (!bodyValue) {
      fprintf(stderr, "ERROR: Failed to compile function body at line %d\n", node->lineNumber);
      free(paramTypes);
      TypeInfer_freeInferredType(inferredType);
      LLVMVariableMap_free(gen->variables);
      gen->variables = prevVars;
      gen->currentFunction = prevFunction;
      return NULL;
    }
  }

  //  Check if return type needs fixing (for polymorphic functions)
  // This must happen BEFORE checking for terminator, because the terminator
  // might have already been created by an explicit <- return statement
  LLVMBasicBlockRef currentBlock = LLVMGetInsertBlock(gen->builder);
  LLVMValueRef terminator = LLVMGetBasicBlockTerminator(currentBlock);

  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] After body compilation: has terminator=%d\n", terminator != NULL);
    #endif
  }

  //  Extract return value type from existing terminator if present
  LLVMTypeRef actualReturnType = NULL;
  if (terminator && LLVMIsAReturnInst(terminator)) {
    LLVMValueRef returnOperand = LLVMGetOperand(terminator, 0);
    if (returnOperand) {
      actualReturnType = LLVMTypeOf(returnOperand);
      if (gen->debugMode) {
        #if 0  // Debug output disabled
        fprintf(stderr, "[DEBUG] Found return terminator with type kind=%d\n",
                (int)LLVMGetTypeKind(actualReturnType));
        #endif
      }
    }
  } else if (!terminator) {
    // No terminator yet - use body value type
    actualReturnType = LLVMTypeOf(bodyValue);
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] No terminator - will add one with type kind=%d\n",
              (int)LLVMGetTypeKind(actualReturnType));
      #endif
    }
  }

  //  If we have an actual return type and it differs from declared type,
  // and the inferred type was UNKNOWN, recreate the function with correct type
  if (gen->debugMode && actualReturnType) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Checking recreation: needsReturnTypeFix=%d, actualReturnType=%p, returnType=%p, match=%d\n",
            needsReturnTypeFix, (void*)actualReturnType, (void*)returnType, actualReturnType == returnType);
    #endif
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] actualReturnType kind=%d, returnType kind=%d\n",
            (int)LLVMGetTypeKind(actualReturnType), (int)LLVMGetTypeKind(returnType));
    #endif
  }

  if (actualReturnType && actualReturnType != returnType &&
      (needsReturnTypeFix ||
       LLVMGetTypeKind(actualReturnType) != LLVMGetTypeKind(returnType))) {
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Return type mismatch: actual=%d expected=%d - recreating function\n",
              (int)LLVMGetTypeKind(actualReturnType), (int)LLVMGetTypeKind(returnType));
      #endif
    }

    // Delete the old function and recreate with correct return type
    LLVMValueRef oldFunction = function;

    // Create new function with correct return type
    LLVMTypeRef correctFuncType = LLVMFunctionType(actualReturnType, paramTypes, paramCount, 0);
    function = LLVMAddFunction(gen->module, tempFuncName, correctFuncType);

    // Move the entry block to the new function
    LLVMBasicBlockRef oldEntry = LLVMGetFirstBasicBlock(oldFunction);
    if (oldEntry) {
      LLVMRemoveBasicBlockFromParent(oldEntry);
      LLVMAppendExistingBasicBlock(function, oldEntry);
      LLVMPositionBuilderAtEnd(gen->builder, oldEntry);
    }

    // Delete the old function
    LLVMDeleteFunction(oldFunction);

    // Update function map with new function
    if (gen->currentFunctionName != NULL) {
      LLVMVariableMap_set(gen->functions, gen->currentFunctionName, function);
    }

    // Update current function reference
    gen->currentFunction = function;
    returnType = actualReturnType;

    // Recreate parameter mappings with new function
    for (int i = 0; i < paramCount; i++) {
      AstNode *paramNode = node->children[i];
      if (paramNode->opcode == OP_IDENTIFIER) {
        LLVMValueRef param = LLVMGetParam(function, i);
        LLVMVariableMap_set(gen->variables, paramNode->val, param);
      }
    }

    // Update terminator reference (since we moved blocks)
    currentBlock = LLVMGetInsertBlock(gen->builder);
    terminator = LLVMGetBasicBlockTerminator(currentBlock);
  }

  // Now handle adding return if needed
  if (!terminator) {
    //  Convert return value to match inferred return type
    actualReturnType = LLVMTypeOf(bodyValue);

    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] No terminator - actualReturnType kind=%d, returnType kind=%d\n",
              (int)LLVMGetTypeKind(actualReturnType), (int)LLVMGetTypeKind(returnType));
      #endif
    }

    //  If inferred type was UNKNOWN, use actual body type (polymorphic functions)
    // This enables functions like {x y -> <- (add x y)} to work with both int and float
    int inferredWasUnknown = (inferredType->returnType == INFER_TYPE_UNKNOWN);

    //  Handle closure returns - if body returns a closure (pointer),
    // the function's return type should be pointer, not i64
    // Type inference doesn't know about closures, so we fix it here
    if (actualReturnType != returnType) {
      LLVMTypeKind actualKind = LLVMGetTypeKind(actualReturnType);
      LLVMTypeKind expectedKind = LLVMGetTypeKind(returnType);

      // If returning a pointer (closure) but expected something else, use pointer type
      if (actualKind == LLVMPointerTypeKind && expectedKind != LLVMPointerTypeKind) {
        // Function returns a closure - override inferred return type
        // This happens with factory functions: {n -> <- {x -> ...}}
        // Just use the actual type (pointer to closure struct)
        returnType = actualReturnType;
      } else if (!inferredWasUnknown) {
        // Only do type conversions if the inferred type was known
        // For UNKNOWN types, we defer to the actual body type
        if (actualReturnType == gen->intType && returnType == gen->floatType) {
          // int → float conversion
          bodyValue = LLVMBuildSIToFP(gen->builder, bodyValue, gen->floatType, "ret_itof");
        } else if (actualReturnType == gen->floatType && returnType == gen->intType) {
          // float → int conversion
          bodyValue = LLVMBuildFPToSI(gen->builder, bodyValue, gen->intType, "ret_ftoi");
        }
      }
    }

    // Add return (function has already been recreated above if needed)
    LLVMBuildRet(gen->builder, bodyValue);
  }

  // Restore previous function and position
  gen->currentFunction = prevFunction;
  gen->isPolymorphicFunction = prevIsPolymorphic;  //  Restore polymorphic flag
  LLVMVariableMap_free(gen->variables);
  gen->variables = prevVars;

  if (prevBlock) {
    LLVMPositionBuilderAtEnd(gen->builder, prevBlock);
  }

  // CRITICAL: Wrap ALL functions in closure structs for uniform representation
  // This ensures dict_map/filter and other higher-order functions work consistently
  // Regular functions get: { funcPtr, NULL env, returnTypeTag }

  // Create closure struct: { i8* funcPtr, i8* envPtr, i32 returnTypeTag }
  LLVMTypeRef closureType = LLVMClosures_getClosureType(gen->context);
  LLVMValueRef closurePtr = LLVMBuildMalloc(gen->builder, closureType, "regular_func_closure");

  // INDUSTRY-STANDARD FIX: Generate wrapper function (Rust/Clang style)
  // Instead of storing function directly, create wrapper that adapts ABI:
  //   Original: i64 func(i64 x)
  //   Wrapper:  i8* wrapper(i8* env, i64 x) { return inttoptr(func(x)) }
  // Need to reconstruct paramTypes for wrapper generator
  LLVMTypeRef funcType = LLVMGlobalGetValueType(function);
  unsigned int funcParamCount = LLVMCountParamTypes(funcType);
  LLVMTypeRef *wrapperParamTypes = malloc(funcParamCount * sizeof(LLVMTypeRef));
  LLVMGetParamTypes(funcType, wrapperParamTypes);
  LLVMTypeRef funcReturnType = LLVMGetReturnType(funcType);

  #if 0  // Debug output disabled
  fprintf(stderr, "[WRAPPER CALL] Generating wrapper for function with %d params\n", funcParamCount);
  #endif
  LLVMValueRef wrapper = createClosureWrapper(gen, function, funcParamCount, wrapperParamTypes, funcReturnType);
  free(wrapperParamTypes);

  // Cast WRAPPER function pointer to i8* (not original function!)
  LLVMTypeRef i8PtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef funcPtrCast = LLVMBuildPointerCast(gen->builder, wrapper, i8PtrType, "wrapper_ptr_cast");

  // Store WRAPPER pointer in field 0
  LLVMValueRef funcIndices[] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0)
  };
  LLVMValueRef funcFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, funcIndices, 2, "func_field");
  LLVMBuildStore(gen->builder, funcPtrCast, funcFieldPtr);

  // Store NULL environment pointer in field 1 (no captured variables)
  LLVMValueRef nullEnv = LLVMConstNull(i8PtrType);
  LLVMValueRef envIndices[] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 1, 0)
  };
  LLVMValueRef envFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, envIndices, 2, "env_field");
  LLVMBuildStore(gen->builder, nullEnv, envFieldPtr);

  // Store return type tag in field 2
  // FIX: Check returnTypeTags map first (trust type inference)
  // If inferred INT/FLOAT, use that tag instead of LLVM type
  // This ensures `add_ten = {x -> <- (add x 10)}` gets tag=0 (INT), not tag=2 (POINTER)
  int returnTypeTag = LLVMClosures_getReturnTypeTag(gen, funcReturnType);  // Default from LLVM type

  // Check if has an inferred tag for this function
  if (gen->currentFunctionName != NULL) {
    void *storedTag = LLVMVariableMap_get(gen->returnTypeTags, gen->currentFunctionName);
    if (storedTag) {
      int inferredTag = (int)((intptr_t)storedTag - 1);  // Stored as tag+1 to avoid NULL

      // PATTERN: Trust inference for INT/FLOAT/CLOSURE/DYNAMIC (industry-standard)
      // This matches OCaml's type-driven compilation and Rust's zero-cost abstractions
      // CRITICAL FIX: Also trust CLOSURE (3) and DYNAMIC (5) tags for higher-order functions
      if (inferredTag == CLOSURE_RETURN_INT || inferredTag == CLOSURE_RETURN_FLOAT ||
          inferredTag == CLOSURE_RETURN_CLOSURE || inferredTag == CLOSURE_RETURN_DYNAMIC) {
        returnTypeTag = inferredTag;
      }
    }
  }

  #if 0  // Debug output disabled
  fprintf(stderr, "[RETURN TYPE TAG] Function return type tag: %d (0=INT, 1=FLOAT, 2=POINTER)\n",
          returnTypeTag);
  #endif
  LLVMValueRef tagValue = LLVMConstInt(LLVMInt32TypeInContext(gen->context), returnTypeTag, 0);
  LLVMValueRef tagIndices[] = {
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0),
    LLVMConstInt(LLVMInt32TypeInContext(gen->context), 2, 0)
  };
  LLVMValueRef tagFieldPtr = LLVMBuildGEP2(gen->builder, closureType, closurePtr, tagIndices, 2, "tag_field");
  LLVMBuildStore(gen->builder, tagValue, tagFieldPtr);

  // CRITICAL FIX: Update returnTypeTags with the ACTUAL return type tag from PASS 2
  // PASS 1 forward declarations may have incorrect/incomplete type inference (e.g., DYNAMIC for functions
  // that return results of other function calls). PASS 2 has the correct type.
  if (gen->currentFunctionName != NULL) {
    if (gen->debugMode) {
      fprintf(stderr, "[PASS 2 UPDATE] Updating returnTypeTags for '%s' with tag=%d\n",
              gen->currentFunctionName, returnTypeTag);
    }
    LLVMVariableMap_set(gen->returnTypeTags, gen->currentFunctionName,
                        (LLVMValueRef)(intptr_t)(returnTypeTag + 1));  // +1 to avoid NULL
  }

  // Convert closure pointer to i64 (universal representation)
  #if 0  // Debug output disabled
  fprintf(stderr, "[FUNC RETURN DEBUG] About to convert closure pointer to i64...\n");
  #endif
  #if 0  // Debug output disabled
  fprintf(stderr, "[FUNC RETURN DEBUG] closurePtr = %p, closurePtr type kind = %d\n",
          closurePtr, LLVMGetTypeKind(LLVMTypeOf(closurePtr)));
  #endif

  LLVMValueRef closureAsInt = LLVMBuildPtrToInt(gen->builder, closurePtr, gen->intType, "closure_as_i64");

  #if 0  // Debug output disabled
  fprintf(stderr, "[FUNC RETURN DEBUG] Converted successfully, closureAsInt type kind = %d\n",
          LLVMGetTypeKind(LLVMTypeOf(closureAsInt)));
  #endif

  #if 0  // Debug output disabled
  fprintf(stderr, "[FUNC RETURN DEBUG] About to free paramTypes and inferredType...\n");
  #endif
  free(paramTypes);
  TypeInfer_freeInferredType(inferredType);  //  Free inferred type

  // CRITICAL FIX: Keep the function in gen->functions for direct calls from other functions
  // The closure wrapper will also be stored in gen->variables by the assignment handler
  // This allows both direct function calls AND first-class function usage
  // Previously we removed from functions map, but this broke calls from nested scopes
  // because gen->variables is reset for each function body compilation

  #if 0  // Debug output disabled
  fprintf(stderr, "[FUNC RETURN DEBUG] About to return closureAsInt...\n");
  #endif
  // Return closure struct as i64 (uniform with LLVMClosures_compileClosure)
  return closureAsInt;
}

// ============================================================================
//  Return Statement Handler
// ============================================================================

LLVMValueRef LLVMCodeGen_compileReturn_impl(LLVMCodeGen *gen, AstNode *node) {
  //  Check if we're inside a loop
  if (gen->loopExitBlock != NULL) {
    // Inside a loop - this is early exit control, not function return
    if (node->childCount == 0) {
      // Return void - continue loop (no early exit)
      // Return NULL to signal void return (no value)
      // The caller (if statement) will handle this as void branch
      return NULL;
    }

    // Compile return value
    LLVMValueRef retValue = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
    if (!retValue) {
      fprintf(stderr, "ERROR: Failed to compile return value at line %d\n", node->lineNumber);
      return NULL;
    }

    // Check if return value is the void constant (0)
    // This handles both (<- void) and (<- 0) where void is defined as 0
    if (LLVMIsConstant(retValue)) {
      if (LLVMIsAConstantInt(retValue)) {
        uint64_t value = LLVMConstIntGetZExtValue(retValue);
        if (value == 0) {
          // Returning void/0 - continue loop, no early exit
          return NULL;
        }
      }
    }

    // Store the return value in loop return pointer (convert to pointer for generic storage)
    if (gen->loopReturnPtr) {
      LLVMTypeRef retType = LLVMTypeOf(retValue);
      LLVMValueRef retPtr;
      
      // Update loop return type tracker
      gen->loopReturnType = retType;
      
      // Convert value to pointer type for storage
      if (retType == gen->intType) {
        retPtr = LLVMBuildIntToPtr(gen->builder, retValue, gen->stringType, "ret_as_ptr");
      } else if (retType == gen->floatType) {
        // Float -> int -> pointer
        LLVMValueRef asInt = LLVMBuildFPToSI(gen->builder, retValue, gen->intType, "float_as_int");
        retPtr = LLVMBuildIntToPtr(gen->builder, asInt, gen->stringType, "ret_as_ptr");
      } else {
        // Already a pointer (string, etc.)
        retPtr = retValue;
      }
      
      LLVMBuildStore(gen->builder, retPtr, gen->loopReturnPtr);
    }

    // Branch to loop exit block (early exit from loop)
    LLVMBuildBr(gen->builder, gen->loopExitBlock);
    
    // Return the value to the caller (for use in if statement type checking)
    // BUT: The current block now has a terminator, so no more instructions should be added
    return retValue;
  }

  // Not in a loop - regular function return
  if (node->childCount == 0) {
    // Return void
    LLVMBuildRetVoid(gen->builder);
    return NULL;
  }

  // TCO: Mark that we're in tail position before compiling return value
  // If the return value is a function call, it will be detected as a tail call
  int prevTailPosition = gen->inTailPosition;
  if (gen->enableTCO) {
    gen->inTailPosition = 1;
  }

  // Compile return value
  LLVMValueRef retValue = LLVMCodeGen_compileNode_impl(gen, node->children[0]);
  if (!retValue) {
    // Restore tail position flag before returning
    if (gen->enableTCO) {
      gen->inTailPosition = prevTailPosition;
    }
    fprintf(stderr, "ERROR: Failed to compile return value at line %d\n", node->lineNumber);
    return NULL;
  }

  // TCO: Restore previous tail position flag
  if (gen->enableTCO) {
    gen->inTailPosition = prevTailPosition;
  }

  LLVMTypeRef retValueType = LLVMTypeOf(retValue);
  LLVMTypeKind retValueKind = LLVMGetTypeKind(retValueType);
  if (gen->currentClosureReturnTag != -1) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[RETURN DEBUG] closureTag=%d, retValueKind=%d\n",
            gen->currentClosureReturnTag, retValueKind);
    #endif
  }
  if (gen->currentClosureReturnTag != -1 && gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[RETURN DEBUG] closureTag=%d, retValueKind=%d\n",
            gen->currentClosureReturnTag, retValueKind);
    #endif
  }
  if (gen->currentClosureReturnTag == CLOSURE_RETURN_FLOAT &&
      retValueKind == LLVMIntegerTypeKind) {
    retValue = LLVMBuildSIToFP(gen->builder, retValue, gen->floatType, "ret_itof_closure");
    retValueType = LLVMTypeOf(retValue);
    retValueKind = LLVMGetTypeKind(retValueType);
  } else if (gen->currentClosureReturnTag == CLOSURE_RETURN_INT &&
             (retValueKind == LLVMFloatTypeKind || retValueKind == LLVMDoubleTypeKind)) {
    retValue = LLVMBuildFPToSI(gen->builder, retValue, gen->intType, "ret_ftoi_closure");
    retValueType = LLVMTypeOf(retValue);
    retValueKind = LLVMGetTypeKind(retValueType);
  }

  //  Convert return value to match function's declared return type
  //  Skip conversion for polymorphic functions (let LLVM use actual type)
  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] Return handler: isPolymorphicFunction=%d, retValue type kind=%d\n",
            gen->isPolymorphicFunction, (int)retValueKind);
    #endif
  }
  if (gen->currentFunction && !gen->isPolymorphicFunction) {
    if (gen->debugMode) {
      #if 0  // Debug output disabled
      fprintf(stderr, "[DEBUG] Return handler: WILL do type conversion check\n");
      #endif
    }
    LLVMTypeRef funcType = LLVMGlobalGetValueType(gen->currentFunction);
    LLVMTypeRef expectedReturnType = LLVMGetReturnType(funcType);
    LLVMTypeRef actualReturnType = LLVMTypeOf(retValue);

    if (actualReturnType != expectedReturnType) {
      //  Handle universal i8* return type for closures
      LLVMTypeKind expectedKind = LLVMGetTypeKind(expectedReturnType);
      LLVMTypeKind actualKind = LLVMGetTypeKind(actualReturnType);

      if (expectedKind == LLVMPointerTypeKind && actualKind == LLVMIntegerTypeKind) {
        // Expected ptr but have int - convert to ptr (closure universal return)
        retValue = LLVMBuildIntToPtr(gen->builder, retValue, expectedReturnType, "ret_int_to_ptr");
      } else if (expectedKind == LLVMPointerTypeKind && actualReturnType == gen->floatType) {
        // Expected ptr but have float - bitcast to int, then to ptr
        LLVMValueRef asInt = LLVMBuildBitCast(gen->builder, retValue, gen->intType, "ret_float_as_int");
        retValue = LLVMBuildIntToPtr(gen->builder, asInt, expectedReturnType, "ret_float_to_ptr");
      } else if (expectedKind == LLVMPointerTypeKind && actualKind == LLVMPointerTypeKind) {
        // Both pointers but different types - simple pointer cast
        retValue = LLVMBuildPointerCast(gen->builder, retValue, expectedReturnType, "ret_ptr_cast");
      } else if (actualReturnType == gen->intType && expectedReturnType == gen->floatType) {
        // int → float conversion
        retValue = LLVMBuildSIToFP(gen->builder, retValue, gen->floatType, "ret_itof");
      } else if (actualReturnType == gen->floatType && expectedReturnType == gen->intType) {
        // float → int conversion
        retValue = LLVMBuildFPToSI(gen->builder, retValue, gen->intType, "ret_ftoi");
      }
    }
  }

  LLVMBuildRet(gen->builder, retValue);
  return retValue;
}

// ============================================================================
// Main Node Dispatcher
// ============================================================================

LLVMValueRef LLVMCodeGen_compileNode_impl(LLVMCodeGen *gen, AstNode *node) {
  if (!node) {
    fprintf(stderr, "ERROR: NULL node\n");
    return NULL;
  }

  switch (node->opcode) {
    case OP_INT:
      return LLVMCodeGen_compileInteger_impl(gen, node);

    case OP_FLOAT:
      return LLVMCodeGen_compileFloat_impl(gen, node);

    case OP_STRING:
      return LLVMCodeGen_compileString_impl(gen, node);

    case OP_IDENTIFIER:
      return LLVMCodeGen_compileVariable_impl(gen, node);

    case OP_ASSIGNMENT:
      return LLVMCodeGen_compileAssignment_impl(gen, node);

    case OP_APPLICATION:
      return LLVMCodeGen_compileApplication_impl(gen, node);

    case OP_STATEMENT:
      return LLVMCodeGen_compileStatement_impl(gen, node);

    case OP_FUNCTION:
      return LLVMCodeGen_compileFunction_impl(gen, node);

    case OP_RETURN:
      return LLVMCodeGen_compileReturn_impl(gen, node);

    case OP_LIST:
      //  List literal [elem1, elem2, ...]
      return LLVMLists_compileList(gen, node);

    default:
      fprintf(stderr, "ERROR: Unsupported opcode %d at line %d\n",
              node->opcode, node->lineNumber);
      fprintf(stderr, " supports: int, float, string, variable, assignment, application, statement, function, return, list\n");
      return NULL;
  }
}

// ============================================================================
//  Forward Declaration Support for Mutual Recursion
// ============================================================================

//  PASS 1 - Create forward declarations for all top-level functions
// This allows functions to call each other (mutual recursion)
static void createForwardDeclarations(LLVMCodeGen *gen, AstNode *ast) {
  if (!ast) return;

  // Only process statement nodes (top-level scope)
  if (ast->opcode != OP_STATEMENT) return;

  // Scan all top-level statements for function assignments
  for (int i = 0; i < ast->childCount; i++) {
    AstNode *child = ast->children[i];

    // Look for: name = {params -> body}
    if (child && child->opcode == OP_ASSIGNMENT && child->childCount >= 2) {
      AstNode *varNode = child->children[0];  // Variable name
      AstNode *valueNode = child->children[1]; // Function definition

      // Check if this is a function assignment
      if (valueNode && valueNode->opcode == OP_FUNCTION && varNode && varNode->val) {
        //  TCO: Create forward declarations for ALL functions (including closures)
        // This enables self-recursion and mutual recursion
        // Previously we skipped closures, but self-recursive functions need forward declarations
        // even if they capture variables (the function's own name appears as a free variable)
        if (gen->debugMode && LLVMClosures_isClosure(valueNode)) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[DEBUG] Creating forward declaration for closure (for recursion): %s\n", varNode->val);
          #endif
        }

        // B: Properly count parameters (same fix as regular functions)
        // Parameters are ALWAYS OP_IDENTIFIER, body statements are OP_STATEMENT/OP_RETURN
        int paramCount = 0;
        while (paramCount < valueNode->childCount &&
               valueNode->children[paramCount]->opcode == OP_IDENTIFIER) {
          paramCount++;
        }

        if (gen->debugMode) {
          #if 0  // Debug output disabled
          fprintf(stderr, "[DEBUG] Forward decl '%s': %d parameters, %d total children\n",
                  varNode->val, paramCount, valueNode->childCount);
          #endif
        }

        // Infer function type for correct signature
        InferredFunctionType *inferredType = TypeInfer_inferFunction(valueNode);
        if (!inferredType) {
          fprintf(stderr, "Warning: Failed to infer type for function '%s' at line %d\n",
                  varNode->val, child->lineNumber);
          continue;
        }

        // Build parameter types
        LLVMTypeRef *paramTypes = malloc(paramCount * sizeof(LLVMTypeRef));
        for (int j = 0; j < paramCount; j++) {
          paramTypes[j] = TypeInfer_getLLVMType(inferredType->paramTypes[j], gen->context);
        }

        // Create function type with inferred return type
        LLVMTypeRef returnType = TypeInfer_getLLVMType(inferredType->returnType, gen->context);

        //  If function returns a closure, override return type to pointer
        // B: For multi-statement bodies, check the LAST statement for closure return
        AstNode *lastBodyStmt = valueNode->children[valueNode->childCount - 1];
        AstNode *actualBody = lastBodyStmt;

        // Unwrap STATEMENT nodes - body might be wrapped in a statement
        if (lastBodyStmt && lastBodyStmt->opcode == OP_STATEMENT && lastBodyStmt->childCount > 0) {
          // Body is a statement - check if it contains a single return
          actualBody = lastBodyStmt->children[lastBodyStmt->childCount - 1];  // Last child (return statement)
        }

        if (actualBody && actualBody->opcode == OP_FUNCTION) {
          returnType = LLVMClosures_getClosureType(gen->context);
          returnType = LLVMPointerType(returnType, 0);
        } else if (actualBody && actualBody->opcode == OP_RETURN && actualBody->childCount > 0) {
          AstNode *returnValue = actualBody->children[0];
          if (returnValue && returnValue->opcode == OP_FUNCTION) {
            returnType = LLVMClosures_getClosureType(gen->context);
            returnType = LLVMPointerType(returnType, 0);
          }
        }

        LLVMTypeRef funcType = LLVMFunctionType(returnType, paramTypes, paramCount, 0);

        // Generate unique LLVM function name (must be unique per function)
        static int funcCounter = 0;
        char llvmFuncName[64];
        snprintf(llvmFuncName, sizeof(llvmFuncName), "_franz_lambda_%d", funcCounter++);

        // Create function declaration (no body yet)
        LLVMValueRef function = LLVMAddFunction(gen->module, llvmFuncName, funcType);

        // Register using Franz variable name so it can be called
        LLVMVariableMap_set(gen->functions, varNode->val, function);

        //  Store inferred return type tag for upstream tagging optimization
        // Convert InferredTypeKind → ClosureReturnTypeTag and store in map
        int returnTypeTag = TypeInfer_toReturnTypeTag(inferredType->returnType);
        LLVMVariableMap_set(gen->returnTypeTags, varNode->val,
                            (LLVMValueRef)(intptr_t)(returnTypeTag + 1));  // +1 to avoid NULL

        if (gen->debugMode) {
          fprintf(stderr, "[STORE] Function '%s' → return tag: %d (inferred: %d)\n",
                  varNode->val, returnTypeTag, inferredType->returnType);
        }

        free(paramTypes);
        TypeInfer_freeInferredType(inferredType);
      }
    }
  }
}

// ============================================================================
// Main Compilation Entry Point
// ============================================================================

int LLVMCodeGen_compile_impl(LLVMCodeGen *gen, AstNode *ast, Scope *globalScope) {
  if (!gen || !ast) return -1;

  gen->currentScope = globalScope;

  // Create main function
  LLVMTypeRef mainType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context),
                                          NULL, 0, 0);
  gen->currentFunction = LLVMAddFunction(gen->module, "main", mainType);

  // Create entry basic block
  LLVMBasicBlockRef entry = LLVMAppendBasicBlock(gen->currentFunction, "entry");
  LLVMPositionBuilderAtEnd(gen->builder, entry);

  //  Add 'void' constant to global scope (10 represents void sentinel)
  LLVMValueRef voidValue = LLVMConstInt(gen->intType, 10, 0);
  LLVMVariableMap_set(gen->variables, "void", voidValue);

  // PASS 1: Compile non-function assignments first
  // This ensures variables like `base = 100` exist before we analyze closures that use them
  if (ast && ast->opcode == OP_STATEMENT) {
    for (int i = 0; i < ast->childCount; i++) {
      AstNode *child = ast->children[i];
      // Compile non-function assignments (e.g., base = 100, x = 42)
      if (child && child->opcode == OP_ASSIGNMENT && child->childCount >= 2) {
        AstNode *valueNode = child->children[1];
        // Skip function assignments - they'll be handled in PASS 2
        if (!valueNode || valueNode->opcode != OP_FUNCTION) {
          LLVMCodeGen_compileNode_impl(gen, child);
        }
      }
    }
  }

  //  PASS 2 - Create forward declarations for all top-level functions
  // This enables mutual recursion (functions calling each other)
  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] PASS 2: Creating forward declarations...\n");
    #endif
  }
  createForwardDeclarations(gen, ast);
  if (gen->debugMode) {
    #if 0  // Debug output disabled
    fprintf(stderr, "[DEBUG] PASS 2 complete\n");
    #endif
  }

  //  PASS 3 - Compile the AST (including function bodies)
  // Functions can now reference each other because all are declared
  LLVMCodeGen_compileNode_impl(gen, ast);

  // Return 0
  LLVMBuildRet(gen->builder, LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0));

  // Verify module
  #if 0  // Debug output disabled
  fprintf(stderr, "[DEBUG] About to verify LLVM module...\n");
  #endif
  char *error = NULL;
  if (LLVMVerifyModule(gen->module, LLVMPrintMessageAction, &error)) {
    fprintf(stderr, "ERROR: LLVM module verification failed:\n%s\n", error);
    #if 0  // Debug output disabled
    fprintf(stderr, "\n[DEBUG] Dumping LLVM IR for inspection:\n");
    #endif
    LLVMDumpModule(gen->module);
    LLVMDisposeMessage(error);
    return -1;
  }

  printf("✅  LLVM IR generation complete\n");
  return 0;
}

// ============================================================================
// Public API Wrappers (for external modules like llvm-math)
// ============================================================================

LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node) {
  return LLVMCodeGen_compileNode_impl(gen, node);
}
