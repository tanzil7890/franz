#ifndef FREEVAR_H
#define FREEVAR_H

#include "../ast.h"

//  Free Variable Analysis for Zero-Leak Closures
//
// Analyzes function ASTs to determine which variables are "free" (captured from environment)
// vs "bound" (defined as parameters or locally).
//
// Example:
//   {x -> <- (add x y z)}
//   Bound: ["x"]
//   Free: ["y", "z", "add"]  // Captured from environment
//
// This enables snapshot-based closures that only capture used variables,
// eliminating circular references and memory leaks.

// Analyze a function node and populate its freeVars field
// p_fn: OP_FUNCTION node to analyze
// Returns: number of free variables found
int FreeVar_analyze(AstNode *p_fn);

// Helper: Check if variable name is in bound list
int FreeVar_is_bound(char *name, char **bound, int bound_count);

// Helper: Add variable to free list if not already present
void FreeVar_add_unique(char ***freeVars, int *count, char *name);

//  Check if identifier is a global built-in function (Rust-style)
// Global symbols have external/internal linkage and are NOT captured in closures
// Returns: 1 if global built-in, 0 if local variable
int FreeVar_is_global_builtin(const char *name);

#endif
