#ifndef SCOPE_H
#define SCOPE_H
#include "generic.h"

//  Scoping mode selection
typedef enum {
  SCOPING_DYNAMIC,  // Caller scope as parent (legacy behavior)
  SCOPING_LEXICAL   // Captured environment as parent ()
} ScopingMode;

// Global scoping mode (set via CLI flag or environment variable)
extern ScopingMode g_scoping_mode;

// Helper to get mode name (for debugging/logging)
const char* ScopingMode_name(ScopingMode mode);

//  Industry-standard array-based scope storage
// Each variable has a name, value, and mutability flag
typedef struct ScopeBinding {
  char* name;          // Variable name (for error messages and debugging)
  Generic *value;      // Variable value
  int isMutable;       // 1 if can be reassigned, 0 if immutable (frozen)
} ScopeBinding;

// scope (assigned to every statement)
//  ARRAY-BASED storage for O(1) variable lookup
// Variables are accessed via precomputed offsets (compile-time analysis)
// This is the industry-standard approach used by Rust, C, OCaml, Go, etc.
typedef struct Scope {
  struct Scope *p_parent;

  // Array-based storage ( Performance Optimization)
  ScopeBinding *bindings;  // Array of variable bindings
  int capacity;             // Allocated capacity
  int count;                // Number of variables currently in scope

  int refCount;  // Reference counting for safe closure lifetime
} Scope;

// prototypes
Scope *Scope_new(Scope *);
Scope *Scope_new_with_capacity(Scope *, int capacity);  //  Pre-allocate capacity
void Scope_print(Scope *);
void Scope_set(Scope *, char *, Generic *, int);
void Scope_set_immutable(Scope *, char *, Generic *, int);
void Scope_freeze(Scope *, char *, int);
Generic *Scope_get(Scope *, char *, int);
void Scope_free(Scope *);  // Deprecated: use Scope_release

//  Reference counting API for closure safety
void Scope_retain(Scope *);   // Increment refCount
void Scope_release(Scope *);  // Decrement refCount, free if 0

//  O(1) variable lookup using precomputed offsets
Generic *Scope_get_by_offset(Scope *, int offset);  // Direct offset access (O(1))
int Scope_get_offset(Scope *, char *name);  // Get offset for variable name

//  O(1) variable store using precomputed offsets (for bytecode VM)
void Scope_set_by_offset(Scope *, int offset, Generic *value);  // Direct offset store (O(1))

#endif