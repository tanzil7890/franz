#include <stdlib.h>
#include <string.h>
#include "freevar.h"
#include <stdio.h>
#include "../ast.h"

//  Free Variable Analysis Implementation

//  Rust-style global built-in registry
// These symbols are globally available (external/internal linkage)
// and should NOT be captured in closures
int FreeVar_is_global_builtin(const char *name) {
  if (!name) return 0;

  // Arithmetic operations
  if (strcmp(name, "add") == 0) return 1;
  if (strcmp(name, "subtract") == 0) return 1;
  if (strcmp(name, "multiply") == 0) return 1;
  if (strcmp(name, "divide") == 0) return 1;
  if (strcmp(name, "remainder") == 0) return 1;
  if (strcmp(name, "power") == 0) return 1;

  // Math functions
  if (strcmp(name, "random") == 0) return 1;
  if (strcmp(name, "floor") == 0) return 1;
  if (strcmp(name, "ceil") == 0) return 1;
  if (strcmp(name, "round") == 0) return 1;
  if (strcmp(name, "abs") == 0) return 1;
  if (strcmp(name, "min") == 0) return 1;
  if (strcmp(name, "max") == 0) return 1;
  if (strcmp(name, "sqrt") == 0) return 1;

  // Comparison operators
  if (strcmp(name, "is") == 0) return 1;
  if (strcmp(name, "less_than") == 0) return 1;
  if (strcmp(name, "greater_than") == 0) return 1;

  // Logical operators
  if (strcmp(name, "not") == 0) return 1;
  if (strcmp(name, "and") == 0) return 1;
  if (strcmp(name, "or") == 0) return 1;

  // Control flow
  if (strcmp(name, "if") == 0) return 1;
  if (strcmp(name, "when") == 0) return 1;
  if (strcmp(name, "unless") == 0) return 1;
  if (strcmp(name, "cond") == 0) return 1;
  if (strcmp(name, "loop") == 0) return 1;
  if (strcmp(name, "while") == 0) return 1;
  if (strcmp(name, "break") == 0) return 1;
  if (strcmp(name, "continue") == 0) return 1;

  // Type guards
  if (strcmp(name, "is_int") == 0) return 1;
  if (strcmp(name, "is_float") == 0) return 1;
  if (strcmp(name, "is_string") == 0) return 1;
  if (strcmp(name, "is_list") == 0) return 1;
  if (strcmp(name, "is_function") == 0) return 1;

  // I/O functions
  if (strcmp(name, "println") == 0) return 1;
  if (strcmp(name, "print") == 0) return 1;
  if (strcmp(name, "input") == 0) return 1;

  // Terminal functions
  if (strcmp(name, "rows") == 0) return 1;
  if (strcmp(name, "columns") == 0) return 1;
  if (strcmp(name, "repeat") == 0) return 1;

  // Type conversion
  if (strcmp(name, "integer") == 0) return 1;
  if (strcmp(name, "float") == 0) return 1;
  if (strcmp(name, "string") == 0) return 1;

  // String operations
  if (strcmp(name, "format-int") == 0) return 1;
  if (strcmp(name, "format-float") == 0) return 1;
  if (strcmp(name, "join") == 0) return 1;
  if (strcmp(name, "get") == 0) return 1;  //  Substring/list indexing

  // Dict operations ()
  if (strcmp(name, "dict") == 0) return 1;
  if (strcmp(name, "dict_get") == 0) return 1;
  if (strcmp(name, "dict_set") == 0) return 1;
  if (strcmp(name, "dict_has") == 0) return 1;
  if (strcmp(name, "dict_keys") == 0) return 1;
  if (strcmp(name, "dict_values") == 0) return 1;
  if (strcmp(name, "dict_map") == 0) return 1;
  if (strcmp(name, "dict_filter") == 0) return 1;
  if (strcmp(name, "dict_merge") == 0) return 1;

  // ADT (Algebraic Data Types) functions 
  if (strcmp(name, "variant") == 0) return 1;
  if (strcmp(name, "match") == 0) return 1;
  if (strcmp(name, "variant_tag") == 0) return 1;
  if (strcmp(name, "variant_values") == 0) return 1;

  // Mutable references - 
  if (strcmp(name, "ref") == 0) return 1;
  if (strcmp(name, "deref") == 0) return 1;
  if (strcmp(name, "set!") == 0) return 1;

  return 0;  // Not a global built-in
}

// Helper: Check if name is in bound variables list
int FreeVar_is_bound(char *name, char **bound, int bound_count) {
  for (int i = 0; i < bound_count; i++) {
    if (strcmp(name, bound[i]) == 0) {
      return 1;  // Found in bound list
    }
  }
  return 0;  // Not bound
}

// Helper: Add name to free variables list if not already present
void FreeVar_add_unique(char ***freeVars, int *count, char *name) {
  // Check if already in list
  for (int i = 0; i < *count; i++) {
    if (strcmp((*freeVars)[i], name) == 0) {
      return;  // Already present, don't add duplicate
    }
  }

  // Add new free variable
  *freeVars = (char **) realloc(*freeVars, sizeof(char *) * (*count + 1));
  (*freeVars)[*count] = (char *) malloc(strlen(name) + 1);
  strcpy((*freeVars)[*count], name);
  (*count)++;
}

// Recursive helper to find free variables in an AST subtree
// bound: array of bound variable names (parameters + local vars)
// bound_count: number of bound variables
// freeVars: accumulator for free variables found
// freeVarsCount: accumulator count
void FreeVar_analyze_rec(AstNode *node, char **bound, int bound_count,
                         char ***freeVars, int *freeVarsCount) {
  if (node == NULL) return;

  if (node->opcode == OP_IDENTIFIER) {
    // This is a variable reference
    char *name = node->val;

    //  Rust-style global symbol filtering
    // Skip global built-ins - they have external/internal linkage and are NOT captured
    if (FreeVar_is_global_builtin(name)) {
      return;  // Global built-in - skip, not a free variable
    }

    // If not bound, it's free (captured from outer scope)
    if (!FreeVar_is_bound(name, bound, bound_count)) {
      FreeVar_add_unique(freeVars, freeVarsCount, name);
    }
  } else if (node->opcode == OP_FUNCTION) {
    // Nested function: compute its free vars separately
    FreeVar_analyze(node);

    //  Fix: Propagate nested function's free vars to parent
    // If nested function needs variable 'x', and 'x' is not bound in parent,
    // then parent must also capture 'x' to pass it to the nested function
    for (int i = 0; i < node->freeVarsCount; i++) {
      char *nestedFreeVar = node->freeVars[i];

      //  Skip global built-ins (already filtered in nested analysis)
      // This double-check prevents transitive capture of globals
      if (FreeVar_is_global_builtin(nestedFreeVar)) {
        continue;
      }

      // Only add if not bound in current (parent) function
      if (!FreeVar_is_bound(nestedFreeVar, bound, bound_count)) {
        FreeVar_add_unique(freeVars, freeVarsCount, nestedFreeVar);
      }
    }
  } else if (node->opcode == OP_ASSIGNMENT) {
    // Assignment: x = expr
    // x becomes bound, expr may have free vars
    // Note: For now, we treat assignments as introducing local bindings
    // In reality, Franz has dynamic scoping, so this is an approximation

    //  Array-based AST access
    // Analyze the value expression (right side)
    if (node->childCount >= 2) {
      FreeVar_analyze_rec(node->children[1], bound, bound_count,
                         freeVars, freeVarsCount);
    }

    // Don't treat assignment target as free variable
    // (It's being defined in this scope)
  } else {
    //  Array-based child traversal
    // Recursively analyze children
    for (int i = 0; i < node->childCount; i++) {
      FreeVar_analyze_rec(node->children[i], bound, bound_count, freeVars, freeVarsCount);
    }
  }
}

//  Helper to collect assignment targets from a node tree
// This adds local variable assignments to the bound list so they're not treated as captures
static void FreeVar_collectAssignments(AstNode *node, char ***bound_ptr, int *bound_count_ptr) {
  if (!node) return;

  if (node->opcode == OP_ASSIGNMENT && node->childCount >= 1) {
    // Found an assignment - add target to bound list
    AstNode *target = node->children[0];
    if (target && target->opcode == OP_IDENTIFIER && target->val) {
      // Check if already in bound list
      int already_bound = 0;
      for (int j = 0; j < *bound_count_ptr; j++) {
        if (strcmp((*bound_ptr)[j], target->val) == 0) {
          already_bound = 1;
          break;
        }
      }

      if (!already_bound) {
        *bound_ptr = (char **)realloc(*bound_ptr, sizeof(char *) * (*bound_count_ptr + 1));
        (*bound_ptr)[*bound_count_ptr] = (char *)malloc(strlen(target->val) + 1);
        strcpy((*bound_ptr)[*bound_count_ptr], target->val);
        (*bound_count_ptr)++;
        #if 0  // Debug output disabled
        fprintf(stderr, "[FREEVAR FIX] Added local assignment '%s' to bound list\n", target->val);
        #endif
      }
    }
  }

  // Recursively scan children (but don't descend into nested functions)
  if (node->opcode != OP_FUNCTION) {
    for (int i = 0; i < node->childCount; i++) {
      FreeVar_collectAssignments(node->children[i], bound_ptr, bound_count_ptr);
    }
  }
}

// Main analysis function: analyze OP_FUNCTION node
// Returns: number of free variables found
int FreeVar_analyze(AstNode *p_fn) {
  if (p_fn == NULL || p_fn->opcode != OP_FUNCTION) {
    return 0;  // Not a function node
  }

  // If already analyzed, return existing count
  if (p_fn->freeVars != NULL) {
    return p_fn->freeVarsCount;
  }

  //  Array-based AST - Build list of bound variables (function parameters)
  char **bound = NULL;
  int bound_count = 0;

  // Robust parameter counting: parameters are leading OP_IDENTIFIER nodes
  // Body can be single or multi-statement; do not assume last child is OP_STATEMENT
  int paramCount = 0;
  while (paramCount < p_fn->childCount && p_fn->children[paramCount]->opcode == OP_IDENTIFIER) {
    paramCount++;
  }

  for (int i = 0; i < paramCount; i++) {
    AstNode *param = p_fn->children[i];
    if (param->val != NULL) {
      // This is a parameter - it's bound
      bound = (char **) realloc(bound, sizeof(char *) * (bound_count + 1));
      bound[bound_count] = (char *) malloc(strlen(param->val) + 1);
      strcpy(bound[bound_count], param->val);
      bound_count++;
    }
  }

  //  CRITICAL FIX - Scan function body for assignment targets FIRST
  // Multi-statement closures define local variables via assignments
  // These must be added to bound list BEFORE analyzing for free variables
  // Example: {x -> a = 5; b = 10; <- (add a b)}  // a and b are LOCAL, not captures!

  // Scan all body statements for assignments to build complete bound list
  for (int i = paramCount; i < p_fn->childCount; i++) {
    FreeVar_collectAssignments(p_fn->children[i], &bound, &bound_count);
  }

  //  Last child is function body (OP_STATEMENT)
  // Analyze the body for free variables
  char **freeVars = NULL;
  int freeVarsCount = 0;

  // Analyze all body nodes (from paramCount onward), regardless of opcode
  for (int i = paramCount; i < p_fn->childCount; i++) {
    AstNode *bodyChild = p_fn->children[i];
    FreeVar_analyze_rec(bodyChild, bound, bound_count, &freeVars, &freeVarsCount);
  }

  // Store result in AST node
  p_fn->freeVars = freeVars;
  p_fn->freeVarsCount = freeVarsCount;

  // Debug printout for analysis visibility
  #if 0  // Debug output disabled
  fprintf(stderr, "[FREEVAR] OP_FUNCTION at line %d: params=%d, freeVars=%d\n",
          p_fn->lineNumber, paramCount, freeVarsCount);
  for (int i = 0; i < freeVarsCount; i++) {
    fprintf(stderr, "[FREEVAR]   %s\n", freeVars[i]);
  }
  #endif

  // Free bound variables list (no longer needed)
  for (int i = 0; i < bound_count; i++) {
    free(bound[i]);
  }
  if (bound != NULL) {
    free(bound);
  }

  return freeVarsCount;
}
