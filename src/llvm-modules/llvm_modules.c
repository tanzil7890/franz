#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llvm_modules.h"
#include "../file.h"
#include "../lex.h"
#include "../parse.h"
#include "../llvm-codegen/llvm_codegen.h"

//  LLVM Module System Implementation
// Industry-standard module loading with circular dependency detection

// ============================================================================
// Import Stack for Circular Dependency Detection
// ============================================================================

#define MAX_IMPORT_DEPTH 128

typedef struct {
  char *modulePath;
  int lineNumber;
} ImportEntry;

static ImportEntry importStack[MAX_IMPORT_DEPTH];
static int importStackSize = 0;
static int importStackInitialized = 0;

void LLVMModules_initImportStack(void) {
  if (!importStackInitialized) {
    importStackSize = 0;
    importStackInitialized = 1;
  }
}

int LLVMModules_pushImport(const char *modulePath, int lineNumber) {
  LLVMModules_initImportStack();

  // Check for circular dependency
  for (int i = 0; i < importStackSize; i++) {
    if (strcmp(importStack[i].modulePath, modulePath) == 0) {
      // Circular dependency detected!
      return -1;
    }
  }

  // Check stack overflow
  if (importStackSize >= MAX_IMPORT_DEPTH) {
    fprintf(stderr, "ERROR: Module import depth exceeded maximum (%d) at line %d\n",
            MAX_IMPORT_DEPTH, lineNumber);
    return -1;
  }

  // Push to stack
  importStack[importStackSize].modulePath = strdup(modulePath);
  importStack[importStackSize].lineNumber = lineNumber;
  importStackSize++;

  return 0;
}

void LLVMModules_popImport(const char *modulePath) {
  if (importStackSize > 0) {
    free(importStack[importStackSize - 1].modulePath);
    importStackSize--;
  }
}

void LLVMModules_printCircularChain(const char *finalModule) {
  fprintf(stderr, "\nCircular dependency detected:\n");
  for (int i = 0; i < importStackSize; i++) {
    fprintf(stderr, "  %s (line %d)\n", importStack[i].modulePath, importStack[i].lineNumber);
    if (i < importStackSize - 1) {
      fprintf(stderr, "    ↓ imports\n");
    }
  }
  fprintf(stderr, "    ↓ tries to import\n");
  fprintf(stderr, "  %s ❌ CIRCULAR!\n\n", finalModule);
}

void LLVMModules_freeImportStack(void) {
  for (int i = 0; i < importStackSize; i++) {
    free(importStack[i].modulePath);
  }
  importStackSize = 0;
}

// ============================================================================
// Module Cache for Compiled Symbols
// ============================================================================

#define MAX_CACHED_MODULES 256

typedef struct {
  char *modulePath;
  LLVMVariableMap *symbolMap;
} CachedModuleEntry;

static CachedModuleEntry moduleCache[MAX_CACHED_MODULES];
static int moduleCacheSize = 0;
static int moduleCacheInitialized = 0;

static void initModuleCache(void) {
  if (!moduleCacheInitialized) {
    moduleCacheSize = 0;
    moduleCacheInitialized = 1;
  }
}

int LLVMModules_isCached(const char *modulePath) {
  initModuleCache();

  for (int i = 0; i < moduleCacheSize; i++) {
    if (strcmp(moduleCache[i].modulePath, modulePath) == 0) {
      return 1;
    }
  }
  return 0;
}

void LLVMModules_cache(const char *modulePath, LLVMVariableMap *symbolMap) {
  initModuleCache();

  if (moduleCacheSize >= MAX_CACHED_MODULES) {
    fprintf(stderr, "WARNING: Module cache full, cannot cache %s\n", modulePath);
    return;
  }

  moduleCache[moduleCacheSize].modulePath = strdup(modulePath);
  moduleCache[moduleCacheSize].symbolMap = symbolMap; // Note: Takes ownership
  moduleCacheSize++;
}

LLVMVariableMap *LLVMModules_getCached(const char *modulePath) {
  initModuleCache();

  for (int i = 0; i < moduleCacheSize; i++) {
    if (strcmp(moduleCache[i].modulePath, modulePath) == 0) {
      return moduleCache[i].symbolMap;
    }
  }
  return NULL;
}

void LLVMModules_clearCache(void) {
  for (int i = 0; i < moduleCacheSize; i++) {
    free(moduleCache[i].modulePath);
    // Note: symbolMap is owned by the module, don't free here
  }
  moduleCacheSize = 0;
}

// ============================================================================
// Helper Functions
// ============================================================================

// Check if a function is a stdlib function (should be available to modules)
static int isStdlibFunction(const char *name) {
  // IO functions
  if (strcmp(name, "print") == 0 || strcmp(name, "println") == 0 ||
      strcmp(name, "input") == 0 || strcmp(name, "read_file") == 0 ||
      strcmp(name, "write_file") == 0) return 1;

  // Arithmetic
  if (strcmp(name, "add") == 0 || strcmp(name, "subtract") == 0 ||
      strcmp(name, "multiply") == 0 || strcmp(name, "divide") == 0 ||
      strcmp(name, "remainder") == 0 || strcmp(name, "power") == 0 ||
      strcmp(name, "sqrt") == 0 || strcmp(name, "abs") == 0 ||
      strcmp(name, "min") == 0 || strcmp(name, "max") == 0 ||
      strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 ||
      strcmp(name, "round") == 0 || strcmp(name, "random") == 0) return 1;

  // Comparisons & Logic
  if (strcmp(name, "is") == 0 || strcmp(name, "less_than") == 0 ||
      strcmp(name, "greater_than") == 0 || strcmp(name, "not") == 0 ||
      strcmp(name, "and") == 0 || strcmp(name, "or") == 0) return 1;

  // Control flow
  if (strcmp(name, "if") == 0 || strcmp(name, "when") == 0 ||
      strcmp(name, "unless") == 0 || strcmp(name, "cond") == 0 ||
      strcmp(name, "loop") == 0 || strcmp(name, "while") == 0 ||
      strcmp(name, "break") == 0 || strcmp(name, "continue") == 0) return 1;

  // Type checking
  if (strcmp(name, "is_int") == 0 || strcmp(name, "is_float") == 0 ||
      strcmp(name, "is_string") == 0 || strcmp(name, "is_list") == 0 ||
      strcmp(name, "is_function") == 0) return 1;

  // List operations
  if (strcmp(name, "map") == 0 || strcmp(name, "reduce") == 0 ||
      strcmp(name, "filter") == 0 || strcmp(name, "head") == 0 ||
      strcmp(name, "tail") == 0 || strcmp(name, "cons") == 0 ||
      strcmp(name, "empty?") == 0 || strcmp(name, "length") == 0 ||
      strcmp(name, "nth") == 0 || strcmp(name, "range") == 0) return 1;

  // String operations
  if (strcmp(name, "concat") == 0 || strcmp(name, "substring") == 0 ||
      strcmp(name, "uppercase") == 0 || strcmp(name, "lowercase") == 0 ||
      strcmp(name, "split") == 0 || strcmp(name, "join") == 0) return 1;

  // Terminal
  if (strcmp(name, "rows") == 0 || strcmp(name, "columns") == 0 ||
      strcmp(name, "repeat") == 0) return 1;

  // Conversions
  if (strcmp(name, "integer") == 0 || strcmp(name, "float") == 0 ||
      strcmp(name, "string") == 0 || strcmp(name, "format-int") == 0 ||
      strcmp(name, "format-float") == 0) return 1;

  return 0; // Not a stdlib function
}

// ============================================================================
// Module Loading - use()
// ============================================================================

int LLVMModules_use(LLVMCodeGen *gen, const char *modulePath, int lineNumber) {
  if (!gen || !modulePath) {
    fprintf(stderr, "ERROR: Invalid arguments to LLVMModules_use\n");
    return -1;
  }

  // Check for circular dependency BEFORE loading
  if (LLVMModules_pushImport(modulePath, lineNumber) != 0) {
    LLVMModules_printCircularChain(modulePath);
    fprintf(stderr, "ERROR: Circular dependency detected when importing '%s' at line %d\n",
            modulePath, lineNumber);
    return -1;
  }

  // Check cache first
  if (LLVMModules_isCached(modulePath)) {
    if (gen->debugMode) {
      fprintf(stderr, "[DEBUG] Module '%s' already compiled (cached)\n", modulePath);
    }

    // Restore symbols from cache
    LLVMVariableMap *cachedSymbols = LLVMModules_getCached(modulePath);
    if (cachedSymbols) {
      // Copy symbols into current scope
      // Note: This is a shallow copy - symbols are already compiled LLVM values
      for (int i = 0; i < cachedSymbols->count; i++) {
        if (cachedSymbols->entries[i].name != NULL) {
          LLVMVariableMap_set(gen->variables, cachedSymbols->entries[i].name, cachedSymbols->entries[i].value);
        }
      }
    }

    LLVMModules_popImport(modulePath);
    return 0;
  }

  // Not cached - load and compile the module
  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Loading module '%s' (not cached)\n", modulePath);
  }

  // Read the module file
  char *code = readFile(modulePath, 1); // 1 = true for isModule
  if (!code) {
    fprintf(stderr, "ERROR: Failed to read module file '%s' at line %d\n",
            modulePath, lineNumber);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Get file length for lexer
  int fileLength = strlen(code);

  // Lex the code
  TokenArray *tokens = lex(code, fileLength);
  if (!tokens) {
    fprintf(stderr, "ERROR: Failed to tokenize module '%s' at line %d\n",
            modulePath, lineNumber);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Parse the code
  AstNode *ast = parseProgram(tokens);
  if (!ast) {
    fprintf(stderr, "ERROR: Failed to parse module '%s' at line %d\n",
            modulePath, lineNumber);
    TokenArray_free(tokens);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Save current variable map (will be restored after module compilation)
  LLVMVariableMap *savedVariables = gen->variables;
  LLVMVariableMap *moduleVariables = LLVMVariableMap_new();
  gen->variables = moduleVariables;

  // Compile the module AST into current LLVM module
  LLVMValueRef moduleResult = LLVMCodeGen_compileNode(gen, ast);

  if (!moduleResult) {
    fprintf(stderr, "ERROR: Failed to compile module '%s' at line %d\n",
            modulePath, lineNumber);
    LLVMVariableMap_free(moduleVariables);
    gen->variables = savedVariables;
    AstNode_free(ast);
    TokenArray_free(tokens);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Cache the module's symbols for future imports
  LLVMModules_cache(modulePath, moduleVariables);

  // Copy module symbols into parent scope
  for (int i = 0; i < moduleVariables->count; i++) {
    if (moduleVariables->entries[i].name != NULL) {
      LLVMVariableMap_set(savedVariables, moduleVariables->entries[i].name, moduleVariables->entries[i].value);
    }
  }

  // Restore parent variable map
  gen->variables = savedVariables;

  // Clean up
  AstNode_free(ast);
  TokenArray_free(tokens);
  free(code);

  // Pop from import stack after successful load
  LLVMModules_popImport(modulePath);

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Module '%s' loaded successfully\n", modulePath);
  }

  return 0;
}

int LLVMModules_useMultiple(LLVMCodeGen *gen, const char **modulePaths, int moduleCount, int lineNumber) {
  for (int i = 0; i < moduleCount; i++) {
    if (LLVMModules_use(gen, modulePaths[i], lineNumber) != 0) {
      return -1;
    }
  }
  return 0;
}

// ============================================================================
// Namespace Support - use_as()
// ============================================================================

int LLVMModules_useAs(LLVMCodeGen *gen, const char *modulePath, const char *namespaceName, int lineNumber) {
  if (!gen || !modulePath || !namespaceName) {
    fprintf(stderr, "ERROR: Invalid arguments to LLVMModules_useAs\n");
    return -1;
  }

  // Check for circular dependency BEFORE loading
  if (LLVMModules_pushImport(modulePath, lineNumber) != 0) {
    LLVMModules_printCircularChain(modulePath);
    fprintf(stderr, "ERROR: Circular dependency detected when importing '%s' at line %d\n",
            modulePath, lineNumber);
    return -1;
  }

  // Check cache first (cache key includes namespace to allow different namespaces)
  char cacheKey[512];
  snprintf(cacheKey, sizeof(cacheKey), "%s@%s", modulePath, namespaceName);

  if (LLVMModules_isCached(cacheKey)) {
    if (gen->debugMode) {
      fprintf(stderr, "[DEBUG] Module '%s' with namespace '%s' already compiled (cached)\n",
              modulePath, namespaceName);
    }

    // Restore symbols from cache
    LLVMVariableMap *cachedSymbols = LLVMModules_getCached(cacheKey);
    if (cachedSymbols) {
      // Copy symbols into current scope
      for (int i = 0; i < cachedSymbols->count; i++) {
        if (cachedSymbols->entries[i].name != NULL) {
          LLVMVariableMap_set(gen->variables, cachedSymbols->entries[i].name, cachedSymbols->entries[i].value);
        }
      }
    }

    LLVMModules_popImport(modulePath);
    return 0;
  }

  // Not cached - load and compile the module
  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Loading module '%s' into namespace '%s' (not cached)\n",
            modulePath, namespaceName);
  }

  // Read the module file
  char *code = readFile(modulePath, 1);
  if (!code) {
    fprintf(stderr, "ERROR: Failed to read module file '%s' at line %d\n",
            modulePath, lineNumber);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Get file length for lexer
  int fileLength = strlen(code);

  // Lex the code
  TokenArray *tokens = lex(code, fileLength);
  if (!tokens) {
    fprintf(stderr, "ERROR: Failed to tokenize module '%s' at line %d\n",
            modulePath, lineNumber);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Parse the code
  AstNode *ast = parseProgram(tokens);
  if (!ast) {
    fprintf(stderr, "ERROR: Failed to parse module '%s' at line %d\n",
            modulePath, lineNumber);
    TokenArray_free(tokens);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Save current variable map AND function map
  LLVMVariableMap *savedVariables = gen->variables;
  LLVMVariableMap *savedFunctions = gen->functions;

  // Save the original counts to identify new symbols added by the module
  int originalVarCount = savedVariables->count;
  int originalFuncCount = savedFunctions->count;

  LLVMVariableMap *moduleVariables = LLVMVariableMap_new();
  LLVMVariableMap *moduleFunctions = LLVMVariableMap_new();

  // CRITICAL FIX: Copy ONLY stdlib functions to module scope
  // The module should be isolated from user-defined functions in the parent
  for (int i = 0; i < savedVariables->count; i++) {
    if (savedVariables->entries[i].name != NULL) {
      // Only copy stdlib variables
      if (isStdlibFunction(savedVariables->entries[i].name)) {
        LLVMVariableMap_set(moduleVariables, savedVariables->entries[i].name, savedVariables->entries[i].value);
      }
    }
  }

  for (int i = 0; i < savedFunctions->count; i++) {
    if (savedFunctions->entries[i].name != NULL) {
      const char *name = savedFunctions->entries[i].name;
      // Only copy stdlib functions, NOT user-defined or namespaced functions
      if (isStdlibFunction(name)) {
        LLVMVariableMap_set(moduleFunctions, name, savedFunctions->entries[i].value);
      }
    }
  }

  gen->variables = moduleVariables;
  gen->functions = moduleFunctions;

  // Compile the module AST
  LLVMValueRef moduleResult = LLVMCodeGen_compileNode(gen, ast);

  if (!moduleResult) {
    fprintf(stderr, "ERROR: Failed to compile module '%s' at line %d\n",
            modulePath, lineNumber);
    LLVMVariableMap_free(moduleVariables);
    LLVMVariableMap_free(moduleFunctions);
    gen->variables = savedVariables;
    gen->functions = savedFunctions;
    AstNode_free(ast);
    TokenArray_free(tokens);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Create namespaced variable map (prefix all symbols with namespace_)
  // ONLY include symbols that were added by the module (not stdlib)
  LLVMVariableMap *namespacedVariables = LLVMVariableMap_new();

  int addedCount = 0;

  // Extract module-defined variables (only new symbols, not from parent)
  for (int i = 0; i < moduleVariables->count; i++) {
    if (moduleVariables->entries[i].name != NULL) {
      const char *symbolName = moduleVariables->entries[i].name;

      // Check if this symbol existed in parent scope (compare by name AND value)
      LLVMValueRef parentValue = LLVMVariableMap_get(savedVariables, symbolName);
      if (parentValue && parentValue == moduleVariables->entries[i].value) {
        // Same name and same LLVM value -> it's from parent scope, skip it
        continue;
      }

      // This is a new symbol defined by the module - add with namespace prefix
      char namespacedName[256];
      snprintf(namespacedName, sizeof(namespacedName), "%s_%s", namespaceName, symbolName);

      LLVMVariableMap_set(namespacedVariables, namespacedName, moduleVariables->entries[i].value);
      LLVMVariableMap_set(savedVariables, namespacedName, moduleVariables->entries[i].value);

      addedCount++;
    }
  }

  // Extract module-defined functions (only new symbols, not from parent)
  for (int i = 0; i < moduleFunctions->count; i++) {
    if (moduleFunctions->entries[i].name != NULL) {
      const char *symbolName = moduleFunctions->entries[i].name;

      // Check if this symbol existed in parent scope (compare by name AND value)
      LLVMValueRef parentValue = LLVMVariableMap_get(savedFunctions, symbolName);
      if (parentValue && parentValue == moduleFunctions->entries[i].value) {
        // Same name and same LLVM value -> it's from parent scope, skip it
        continue;
      }

      // This is a new symbol defined by the module - add with namespace prefix
      char namespacedName[256];
      snprintf(namespacedName, sizeof(namespacedName), "%s_%s", namespaceName, symbolName);

      LLVMVariableMap_set(namespacedVariables, namespacedName, moduleFunctions->entries[i].value);
      LLVMVariableMap_set(savedVariables, namespacedName, moduleFunctions->entries[i].value);
      LLVMVariableMap_set(savedFunctions, namespacedName, moduleFunctions->entries[i].value);

      addedCount++;
    }
  }


  // Cache the namespaced symbols
  LLVMModules_cache(cacheKey, namespacedVariables);

  // Restore parent variable and function maps
  gen->variables = savedVariables;
  gen->functions = savedFunctions;

  // Clean up module variables (but not namespacedVariables - it's cached)
  LLVMVariableMap_free(moduleVariables);
  LLVMVariableMap_free(moduleFunctions);
  AstNode_free(ast);
  TokenArray_free(tokens);
  free(code);

  // Pop from import stack after successful load
  LLVMModules_popImport(modulePath);

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Module '%s' loaded into namespace '%s' successfully\n",
            modulePath, namespaceName);
  }

  return 0;
}

// ============================================================================
// Capability-Based Security - use_with()
// ============================================================================

// Capability filtering helper
static int hasCapability(const char **capabilities, int capabilityCount, const char *capability) {
  for (int i = 0; i < capabilityCount; i++) {
    if (strcmp(capabilities[i], capability) == 0) {
      return 1;
    }
  }
  return 0;
}

int LLVMModules_useWith(LLVMCodeGen *gen, const char **capabilities, int capabilityCount,
                         const char *modulePath, int lineNumber) {
  if (!gen || !capabilities || !modulePath) {
    fprintf(stderr, "ERROR: Invalid arguments to LLVMModules_useWith\n");
    return -1;
  }

  // Check for circular dependency BEFORE loading
  if (LLVMModules_pushImport(modulePath, lineNumber) != 0) {
    LLVMModules_printCircularChain(modulePath);
    fprintf(stderr, "ERROR: Circular dependency detected when importing '%s' at line %d\n",
            modulePath, lineNumber);
    return -1;
  }

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Loading module '%s' with %d capabilities\n",
            modulePath, capabilityCount);
    for (int i = 0; i < capabilityCount; i++) {
      fprintf(stderr, "[DEBUG]   - %s\n", capabilities[i]);
    }
  }

  // Read the module file
  char *code = readFile(modulePath, 1);
  if (!code) {
    fprintf(stderr, "ERROR: Failed to read module file '%s' at line %d\n",
            modulePath, lineNumber);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Get file length for lexer
  int fileLength = strlen(code);

  // Lex the code
  TokenArray *tokens = lex(code, fileLength);
  if (!tokens) {
    fprintf(stderr, "ERROR: Failed to tokenize module '%s' at line %d\n",
            modulePath, lineNumber);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Parse the code
  AstNode *ast = parseProgram(tokens);
  if (!ast) {
    fprintf(stderr, "ERROR: Failed to parse module '%s' at line %d\n",
            modulePath, lineNumber);
    TokenArray_free(tokens);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Save current variable and function maps, create restricted scopes
  LLVMVariableMap *savedVariables = gen->variables;
  LLVMVariableMap *savedFunctions = gen->functions;
  LLVMVariableMap *restrictedVariables = LLVMVariableMap_new();
  LLVMVariableMap *restrictedFunctions = LLVMVariableMap_new();

  // Copy only allowed functions based on capabilities (from both maps)
  for (int i = 0; i < savedVariables->count; i++) {
    if (savedVariables->entries[i].name != NULL) {
      const char *name = savedVariables->entries[i].name;
      int allowed = 0;

      // Check capabilities and allow corresponding functions
      // Capability: "io" - allows print, println, input, read_file, write_file
      if (hasCapability(capabilities, capabilityCount, "io")) {
        if (strcmp(name, "print") == 0 || strcmp(name, "println") == 0 ||
            strcmp(name, "input") == 0 || strcmp(name, "read_file") == 0 ||
            strcmp(name, "write_file") == 0 || strcmp(name, "shell") == 0) {
          allowed = 1;
        }
      }

      // Capability: "math" - allows arithmetic and math functions
      if (hasCapability(capabilities, capabilityCount, "math")) {
        if (strcmp(name, "add") == 0 || strcmp(name, "subtract") == 0 ||
            strcmp(name, "multiply") == 0 || strcmp(name, "divide") == 0 ||
            strcmp(name, "remainder") == 0 || strcmp(name, "power") == 0 ||
            strcmp(name, "sqrt") == 0 || strcmp(name, "abs") == 0 ||
            strcmp(name, "min") == 0 || strcmp(name, "max") == 0 ||
            strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 ||
            strcmp(name, "round") == 0 || strcmp(name, "random") == 0) {
          allowed = 1;
        }
      }

      // Capability: "logic" - allows comparisons and logical operators
      if (hasCapability(capabilities, capabilityCount, "logic")) {
        if (strcmp(name, "is") == 0 || strcmp(name, "less_than") == 0 ||
            strcmp(name, "greater_than") == 0 || strcmp(name, "and") == 0 ||
            strcmp(name, "or") == 0 || strcmp(name, "not") == 0 ||
            strcmp(name, "if") == 0 || strcmp(name, "when") == 0 ||
            strcmp(name, "unless") == 0) {
          allowed = 1;
        }
      }

      // Capability: "list" - allows list operations
      if (hasCapability(capabilities, capabilityCount, "list")) {
        if (strcmp(name, "map") == 0 || strcmp(name, "reduce") == 0 ||
            strcmp(name, "filter") == 0 || strcmp(name, "head") == 0 ||
            strcmp(name, "tail") == 0 || strcmp(name, "cons") == 0 ||
            strcmp(name, "empty?") == 0 || strcmp(name, "length") == 0 ||
            strcmp(name, "nth") == 0 || strcmp(name, "is_list") == 0 ||
            strcmp(name, "range") == 0) {
          allowed = 1;
        }
      }

      // Capability: "string" - allows string operations
      if (hasCapability(capabilities, capabilityCount, "string")) {
        if (strcmp(name, "concat") == 0 || strcmp(name, "substring") == 0 ||
            strcmp(name, "uppercase") == 0 || strcmp(name, "lowercase") == 0 ||
            strcmp(name, "split") == 0 || strcmp(name, "join") == 0 ||
            strcmp(name, "is_string") == 0) {
          allowed = 1;
        }
      }

      // Capability: "type" - allows type checking
      if (hasCapability(capabilities, capabilityCount, "type")) {
        if (strcmp(name, "is_int") == 0 || strcmp(name, "is_float") == 0 ||
            strcmp(name, "is_string") == 0 || strcmp(name, "is_list") == 0 ||
            strcmp(name, "is_function") == 0) {
          allowed = 1;
        }
      }

      // Capability: "all" - allows everything (bypass restrictions)
      if (hasCapability(capabilities, capabilityCount, "all")) {
        allowed = 1;
      }

      if (allowed) {
        LLVMVariableMap_set(restrictedVariables, name, savedVariables->entries[i].value);
      }
    }
  }

  // Copy allowed functions from functions map
  for (int i = 0; i < savedFunctions->count; i++) {
    if (savedFunctions->entries[i].name != NULL) {
      const char *name = savedFunctions->entries[i].name;
      int allowed = 0;

      // Check capabilities (same logic as above)
      if (hasCapability(capabilities, capabilityCount, "io")) {
        if (strcmp(name, "print") == 0 || strcmp(name, "println") == 0 ||
            strcmp(name, "input") == 0 || strcmp(name, "read_file") == 0 ||
            strcmp(name, "write_file") == 0 || strcmp(name, "shell") == 0) {
          allowed = 1;
        }
      }

      if (hasCapability(capabilities, capabilityCount, "math")) {
        if (strcmp(name, "add") == 0 || strcmp(name, "subtract") == 0 ||
            strcmp(name, "multiply") == 0 || strcmp(name, "divide") == 0 ||
            strcmp(name, "remainder") == 0 || strcmp(name, "power") == 0 ||
            strcmp(name, "sqrt") == 0 || strcmp(name, "abs") == 0 ||
            strcmp(name, "min") == 0 || strcmp(name, "max") == 0 ||
            strcmp(name, "floor") == 0 || strcmp(name, "ceil") == 0 ||
            strcmp(name, "round") == 0 || strcmp(name, "random") == 0) {
          allowed = 1;
        }
      }

      if (hasCapability(capabilities, capabilityCount, "logic")) {
        if (strcmp(name, "is") == 0 || strcmp(name, "less_than") == 0 ||
            strcmp(name, "greater_than") == 0 || strcmp(name, "and") == 0 ||
            strcmp(name, "or") == 0 || strcmp(name, "not") == 0 ||
            strcmp(name, "if") == 0 || strcmp(name, "when") == 0 ||
            strcmp(name, "unless") == 0) {
          allowed = 1;
        }
      }

      if (hasCapability(capabilities, capabilityCount, "list")) {
        if (strcmp(name, "map") == 0 || strcmp(name, "reduce") == 0 ||
            strcmp(name, "filter") == 0 || strcmp(name, "head") == 0 ||
            strcmp(name, "tail") == 0 || strcmp(name, "cons") == 0 ||
            strcmp(name, "empty?") == 0 || strcmp(name, "length") == 0 ||
            strcmp(name, "nth") == 0 || strcmp(name, "is_list") == 0 ||
            strcmp(name, "range") == 0) {
          allowed = 1;
        }
      }

      if (hasCapability(capabilities, capabilityCount, "string")) {
        if (strcmp(name, "concat") == 0 || strcmp(name, "substring") == 0 ||
            strcmp(name, "uppercase") == 0 || strcmp(name, "lowercase") == 0 ||
            strcmp(name, "split") == 0 || strcmp(name, "join") == 0 ||
            strcmp(name, "is_string") == 0) {
          allowed = 1;
        }
      }

      if (hasCapability(capabilities, capabilityCount, "type")) {
        if (strcmp(name, "is_int") == 0 || strcmp(name, "is_float") == 0 ||
            strcmp(name, "is_string") == 0 || strcmp(name, "is_list") == 0 ||
            strcmp(name, "is_function") == 0) {
          allowed = 1;
        }
      }

      if (hasCapability(capabilities, capabilityCount, "all")) {
        allowed = 1;
      }

      if (allowed) {
        LLVMVariableMap_set(restrictedFunctions, name, savedFunctions->entries[i].value);
      }
    }
  }

  // Set restricted scopes
  gen->variables = restrictedVariables;
  gen->functions = restrictedFunctions;

  // Compile the module AST with restricted capabilities
  LLVMValueRef moduleResult = LLVMCodeGen_compileNode(gen, ast);

  if (!moduleResult) {
    fprintf(stderr, "ERROR: Failed to compile module '%s' at line %d\n",
            modulePath, lineNumber);
    LLVMVariableMap_free(restrictedVariables);
    LLVMVariableMap_free(restrictedFunctions);
    gen->variables = savedVariables;
    gen->functions = savedFunctions;
    AstNode_free(ast);
    TokenArray_free(tokens);
    free(code);
    LLVMModules_popImport(modulePath);
    return -1;
  }

  // Restore full scopes
  gen->variables = savedVariables;
  gen->functions = savedFunctions;

  // Copy module-defined symbols to parent scope (from both variables and functions)
  for (int i = 0; i < restrictedVariables->count; i++) {
    if (restrictedVariables->entries[i].name != NULL) {
      // Only copy symbols that are not in the original saved scope
      // (i.e., symbols defined by the module, not stdlib)
      LLVMValueRef existingValue = LLVMVariableMap_get(savedVariables, restrictedVariables->entries[i].name);
      if (!existingValue) {
        LLVMVariableMap_set(savedVariables, restrictedVariables->entries[i].name, restrictedVariables->entries[i].value);
      }
    }
  }

  for (int i = 0; i < restrictedFunctions->count; i++) {
    if (restrictedFunctions->entries[i].name != NULL) {
      LLVMValueRef existingFunc = LLVMVariableMap_get(savedFunctions, restrictedFunctions->entries[i].name);
      if (!existingFunc) {
        LLVMVariableMap_set(savedFunctions, restrictedFunctions->entries[i].name, restrictedFunctions->entries[i].value);
      }
    }
  }

  // Clean up
  LLVMVariableMap_free(restrictedVariables);
  LLVMVariableMap_free(restrictedFunctions);
  AstNode_free(ast);
  TokenArray_free(tokens);
  free(code);

  // Pop from import stack after successful load
  LLVMModules_popImport(modulePath);

  if (gen->debugMode) {
    fprintf(stderr, "[DEBUG] Module '%s' loaded with restricted capabilities\n", modulePath);
  }

  return 0;
}
