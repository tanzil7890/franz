#ifndef CLOSURE_H
#define CLOSURE_H

#include "../ast.h"
#include "../scope.h"
#include "../dict.h"

// Closure: function + captured variables
//  Snapshot-Based Closures (Zero Memory Leaks)
//
// A closure packages together:
// - p_fn: The function's AST (code to execute)
// - p_snapshot: Dictionary of ONLY the free variables this function uses
//
// This enables lexical scoping WITHOUT circular references:
// - Closures snapshot only used variables (not entire scope)
// - No scope -> closure -> scope cycles
// - Zero memory leaks ()
//
// Example:
//   Scope has: x=10, y=20, z=30
//   Function: {-> <- (add x y)}
//   Snapshot: {"x": 10, "y": 20}  (only x and y, not z or scope itself)
typedef struct Closure {
  AstNode *p_fn;       // Function AST node
  Dict *p_snapshot;    // Captured variables (NOT full scope - zero leaks!)
} Closure;

// Prototypes

// Create new closure with function AST and snapshot of free variables
// Analyzes p_fn to determine which variables to capture from p_scope
// Creates Dict snapshot of ONLY those variables (no circular refs)
Closure *Closure_new(AstNode *p_fn, Scope *p_scope);

// Free closure and its snapshot
// Decrements refCounts of captured values, frees Dict
void Closure_free(Closure *p_closure);

// Copy closure (copies AST and snapshot Dict)
Closure *Closure_copy(Closure *p_closure);

#endif
