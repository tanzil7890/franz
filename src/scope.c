#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scope.h"
#include "generic.h"

//  Global scoping mode (defaults to lexical for production)
ScopingMode g_scoping_mode = SCOPING_LEXICAL;

//  Get scoping mode name
const char* ScopingMode_name(ScopingMode mode) {
  switch (mode) {
    case SCOPING_DYNAMIC: return "dynamic";
    case SCOPING_LEXICAL: return "lexical";
    default: return "unknown";
  }
}

// ==============================================================================
//  Array-Based Scope Implementation (Industry Standard)
// ==============================================================================

#define SCOPE_INITIAL_CAPACITY 8  // Start with 8 slots, grow as needed

// Creates a new scope with default capacity
Scope *Scope_new(Scope *p_parent) {
  return Scope_new_with_capacity(p_parent, SCOPE_INITIAL_CAPACITY);
}

// Creates a new scope with specified capacity (for optimization)
Scope *Scope_new_with_capacity(Scope *p_parent, int capacity) {
  Scope *res = (Scope *) malloc(sizeof(Scope));
  res->p_parent = p_parent;
  res->refCount = 1;
  res->capacity = capacity;
  res->count = 0;

  // Allocate array for bindings
  res->bindings = (ScopeBinding *) malloc(sizeof(ScopeBinding) * capacity);

  // Initialize all bindings to NULL
  for (int i = 0; i < capacity; i++) {
    res->bindings[i].name = NULL;
    res->bindings[i].value = NULL;
    res->bindings[i].isMutable = 1;
  }

  return res;
}

// Print scope contents (for debugging)
void Scope_print(Scope *p_in) {
  if (p_in->p_parent == NULL) {
    printf("Global Scope (%d/%d variables):\n", p_in->count, p_in->capacity);
  } else {
    printf("Local Scope (%d/%d variables):\n", p_in->count, p_in->capacity);
  }

  for (int i = 0; i < p_in->count; i++) {
    printf("  [%d] %s = ", i, p_in->bindings[i].name);
    Generic_print(p_in->bindings[i].value);
    if (!p_in->bindings[i].isMutable) {
      printf(" (frozen)");
    }
    printf("\n");
  }
}

// Helper: Find variable by name, returns offset (-1 if not found)
int Scope_get_offset(Scope *p_target, char *name) {
  for (int i = 0; i < p_target->count; i++) {
    if (strcmp(p_target->bindings[i].name, name) == 0) {
      return i;
    }
  }
  return -1;
}

// Helper: Grow bindings array when capacity is reached
static void Scope_grow(Scope *p_target) {
  int new_capacity = p_target->capacity * 2;
  ScopeBinding *new_bindings = (ScopeBinding *) malloc(sizeof(ScopeBinding) * new_capacity);

  // Copy existing bindings
  for (int i = 0; i < p_target->count; i++) {
    new_bindings[i] = p_target->bindings[i];
  }

  // Initialize new slots
  for (int i = p_target->count; i < new_capacity; i++) {
    new_bindings[i].name = NULL;
    new_bindings[i].value = NULL;
    new_bindings[i].isMutable = 1;
  }

  free(p_target->bindings);
  p_target->bindings = new_bindings;
  p_target->capacity = new_capacity;
}

// Set variable in scope (creates new or updates existing)
void Scope_set(Scope *p_target, char *key, Generic *p_val, int lineNumber) {
  p_val->refCount++;

  // Check if variable already exists
  int offset = Scope_get_offset(p_target, key);

  if (offset == -1) {
    // Variable doesn't exist - create new binding
    if (p_target->count >= p_target->capacity) {
      Scope_grow(p_target);
    }

    // Add at next available slot
    offset = p_target->count;
    p_target->bindings[offset].name = (char *) malloc(strlen(key) + 1);
    strcpy(p_target->bindings[offset].name, key);
    p_target->bindings[offset].value = p_val;
    p_target->bindings[offset].isMutable = 1;
    p_target->count++;

  } else {
    // Variable exists - update it
    ScopeBinding *binding = &p_target->bindings[offset];

    // Check mutability (if lineNumber >= 0)
    if (lineNumber >= 0 && !binding->isMutable) {
      printf(
        "Runtime Error @ Line %i: Cannot reassign frozen variable '%s'.\n",
        lineNumber, key
      );
      exit(0);
    }

    // Decrement old value refcount
    binding->value->refCount--;
    if (binding->value->refCount == 0) {
      Generic_free(binding->value);
    }

    // Set new value
    binding->value = p_val;
  }
}

// Set variable as immutable (for initial binding)
void Scope_set_immutable(Scope *p_target, char *key, Generic *p_val, int lineNumber) {
  Scope_set(p_target, key, p_val, lineNumber);

  int offset = Scope_get_offset(p_target, key);
  if (offset >= 0) {
    p_target->bindings[offset].isMutable = 0;
  }
}

// Freeze an existing variable (make it immutable)
// Searches current scope and parent scopes
void Scope_freeze(Scope *p_target, char *var_name, int lineNumber) {
  int offset = Scope_get_offset(p_target, var_name);

  if (offset != -1) {
    // Found in current scope
    p_target->bindings[offset].isMutable = 0;
    return;
  }

  // Not in current scope - check parent
  if (p_target->p_parent == NULL) {
    // Variable not found anywhere
    printf(
      "Runtime Error @ Line %i: Cannot freeze undefined variable '%s'.\n",
      lineNumber, var_name
    );
    exit(0);
  } else {
    // Recursively search parent
    Scope_freeze(p_target->p_parent, var_name, lineNumber);
  }
}

// Get variable by name (with parent scope traversal)
Generic *Scope_get(Scope *p_target, char *key, int lineNumber) {
  int offset = Scope_get_offset(p_target, key);

  if (offset == -1) {
    // Variable not in current scope - check parent
    if (p_target->p_parent == NULL) {
      // In global scope and not found
      if (lineNumber == -1) {
        return NULL;  // Optional lookup
      }

      printf(
        "Runtime Error @ Line %i: %s is not defined.\n",
        lineNumber, key
      );
      exit(0);
    } else {
      // Search parent scope
      return Scope_get(p_target->p_parent, key, lineNumber);
    }
  }

  return p_target->bindings[offset].value;
}

//  Direct offset access (O(1) - no string comparison)
Generic *Scope_get_by_offset(Scope *p_target, int offset) {
  if (offset < 0 || offset >= p_target->count) {
    fprintf(stderr, "Internal Error: Invalid offset %d (count=%d)\n", offset, p_target->count);
    exit(1);
  }

  return p_target->bindings[offset].value;
}

//  Direct offset store (O(1) - for bytecode VM)
void Scope_set_by_offset(Scope *p_target, int offset, Generic *value) {
  if (offset < 0 || offset >= p_target->count) {
    fprintf(stderr, "Internal Error: Invalid offset %d (count=%d)\n", offset, p_target->count);
    exit(1);
  }

  // Check mutability
  if (!p_target->bindings[offset].isMutable) {
    fprintf(stderr, "Runtime Error: Cannot reassign immutable variable at offset %d\n", offset);
    exit(1);
  }

  // Release old value
  Generic *old = p_target->bindings[offset].value;
  if (old != NULL) {
    old->refCount--;
    if (old->refCount == 0) {
      Generic_free(old);
    }
  }

  // Store new value
  p_target->bindings[offset].value = value;
  value->refCount++;
}

//  Reference counting
void Scope_retain(Scope *p_target) {
  if (p_target == NULL) return;
  p_target->refCount++;
}

void Scope_release(Scope *p_target) {
  if (p_target == NULL) return;

  p_target->refCount--;

  if (p_target->refCount == 0) {
    // Free all bindings
    for (int i = 0; i < p_target->count; i++) {
      free(p_target->bindings[i].name);

      p_target->bindings[i].value->refCount--;
      if (p_target->bindings[i].value->refCount == 0) {
        Generic_free(p_target->bindings[i].value);
      }
    }

    free(p_target->bindings);

    // Don't release parent (closures manage parent lifetime explicitly)

    free(p_target);
  }
}

// Deprecated: use Scope_release
void Scope_free(Scope *p_target) {
  Scope_release(p_target);
}
