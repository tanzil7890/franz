#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "closure.h"
#include "../scope.h"
#include "../ast.h"
#include "../dict.h"
#include "../freevar/freevar.h"
#include "../generic.h"

//  Snapshot-Based Closure Implementation (Zero Leaks)

// Create new closure with snapshot of only free variables
// p_fn: The function's AST node (will be copied and analyzed for free vars)
// p_scope: The scope to snapshot free variables from
Closure *Closure_new(AstNode *p_fn, Scope *p_scope) {
  Closure *p_closure = (Closure *) malloc(sizeof(Closure));

  // Copy the function AST (closure owns its own copy)
  p_closure->p_fn = AstNode_copy(p_fn, 0);

  // Analyze function to find free variables 
  FreeVar_analyze(p_closure->p_fn);

  // Create snapshot dict of ONLY the free variables
  // Initial capacity: number of free vars (or 8 if zero)
  int capacity = p_closure->p_fn->freeVarsCount > 0 ? p_closure->p_fn->freeVarsCount : 8;
  p_closure->p_snapshot = Dict_new(capacity);

  // For each free variable, capture its current value from scope
  for (int i = 0; i < p_closure->p_fn->freeVarsCount; i++) {
    char *varName = p_closure->p_fn->freeVars[i];

    // Look up variable in scope (may return NULL if not defined)
    Generic *val = Scope_get(p_scope, varName, -1);  // -1 = no error if missing

    if (val != NULL) {
      // Create Generic string key for Dict
      char **keyPtr = (char **) malloc(sizeof(char *));
      *keyPtr = (char *) malloc(strlen(varName) + 1);
      strcpy(*keyPtr, varName);
      Generic *key = Generic_new(TYPE_STRING, keyPtr, 0);

      // Store in snapshot (Dict_set_inplace makes copies of both key and value)
      // No need to increment refCount - Dict_set_inplace creates its own copies
      Dict_set_inplace(p_closure->p_snapshot, key, val);

      // Free the original key (Dict_set_inplace made a copy)
      Generic_free(key);
    }
    // If variable not found, it's probably a builtin (like "add", "print")
    // We don't snapshot builtins - they're looked up at runtime from global scope
  }

  return p_closure;
}

// Free closure and its snapshot
void Closure_free(Closure *p_closure) {
  if (p_closure == NULL) return;

  #ifdef DEBUG_CLOSURE_FREE
  fprintf(stderr, "[DEBUG] Closure_free: Freeing closure with %d free vars\n",
          p_closure->p_fn ? p_closure->p_fn->freeVarsCount : 0);
  #endif

  // Free the function AST
  if (p_closure->p_fn != NULL) {
    AstNode_free(p_closure->p_fn);
  }

  // Free the snapshot (Dict_free will handle refCount decrement)
  if (p_closure->p_snapshot != NULL) {
    #ifdef DEBUG_CLOSURE_FREE
    fprintf(stderr, "[DEBUG] Closure_free: Freeing snapshot Dict\n");
    #endif
    Dict_free(p_closure->p_snapshot);
  }

  // Free the closure struct itself
  free(p_closure);

  #ifdef DEBUG_CLOSURE_FREE
  fprintf(stderr, "[DEBUG] Closure_free: Done\n");
  #endif
}

// Copy closure (creates new closure with copied AST and copied snapshot)
Closure *Closure_copy(Closure *p_closure) {
  if (p_closure == NULL) return NULL;

  Closure *p_new = (Closure *) malloc(sizeof(Closure));

  // Copy the function AST
  p_new->p_fn = AstNode_copy(p_closure->p_fn, 0);

  // Copy the snapshot Dict (Dict_copy handles refCount increment)
  p_new->p_snapshot = Dict_copy(p_closure->p_snapshot);

  return p_new;
}
