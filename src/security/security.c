#include "security.h"
#include "../stdlib.h"
#include "../list.h"
#include <stdio.h>
#include <string.h>

// Forward declarations from stdlib.c
extern Generic *StdLib_print(Scope *, Generic *[], int, int);
extern Generic *StdLib_println(Scope *, Generic *[], int, int);
extern Generic *StdLib_input(Scope *, Generic *[], int, int);
extern Generic *StdLib_read_file(Scope *, Generic *[], int, int);
extern Generic *StdLib_write_file(Scope *, Generic *[], int, int);
extern Generic *StdLib_shell(Scope *, Generic *[], int, int);
extern Generic *StdLib_event(Scope *, Generic *[], int, int);
extern Generic *StdLib_rows(Scope *, Generic *[], int, int);
extern Generic *StdLib_columns(Scope *, Generic *[], int, int);
extern Generic *StdLib_wait(Scope *, Generic *[], int, int);
extern Generic *StdLib_time(Scope *, Generic *[], int, int);
extern Generic *StdLib_random(Scope *, Generic *[], int, int);
extern Generic *StdLib_is(Scope *, Generic *[], int, int);
extern Generic *StdLib_less_than(Scope *, Generic *[], int, int);
extern Generic *StdLib_greater_than(Scope *, Generic *[], int, int);
extern Generic *StdLib_not(Scope *, Generic *[], int, int);
extern Generic *StdLib_and(Scope *, Generic *[], int, int);
extern Generic *StdLib_or(Scope *, Generic *[], int, int);
extern Generic *StdLib_add(Scope *, Generic *[], int, int);
extern Generic *StdLib_subtract(Scope *, Generic *[], int, int);
extern Generic *StdLib_divide(Scope *, Generic *[], int, int);
extern Generic *StdLib_multiply(Scope *, Generic *[], int, int);
extern Generic *StdLib_remainder(Scope *, Generic *[], int, int);
extern Generic *StdLib_power(Scope *, Generic *[], int, int);
extern Generic *StdLib_loop(Scope *, Generic *[], int, int);
extern Generic *StdLib_until(Scope *, Generic *[], int, int);
extern Generic *StdLib_if(Scope *, Generic *[], int, int);
extern Generic *StdLib_integer(Scope *, Generic *[], int, int);
extern Generic *StdLib_string(Scope *, Generic *[], int, int);
extern Generic *StdLib_float(Scope *, Generic *[], int, int);
extern Generic *StdLib_type(Scope *, Generic *[], int, int);
extern Generic *StdLib_list(Scope *, Generic *[], int, int);
extern Generic *StdLib_length(Scope *, Generic *[], int, int);
extern Generic *StdLib_join(Scope *, Generic *[], int, int);
extern Generic *StdLib_get(Scope *, Generic *[], int, int);
extern Generic *StdLib_insert(Scope *, Generic *[], int, int);
extern Generic *StdLib_set(Scope *, Generic *[], int, int);
extern Generic *StdLib_delete(Scope *, Generic *[], int, int);
extern Generic *StdLib_map(Scope *, Generic *[], int, int);
extern Generic *StdLib_reduce(Scope *, Generic *[], int, int);
extern Generic *StdLib_filter(Scope *, Generic *[], int, int);
extern Generic *StdLib_range(Scope *, Generic *[], int, int);
extern Generic *StdLib_find(Scope *, Generic *[], int, int);
extern Generic *StdLib_head(Scope *, Generic *[], int, int);
extern Generic *StdLib_tail(Scope *, Generic *[], int, int);
extern Generic *StdLib_cons(Scope *, Generic *[], int, int);
extern Generic *StdLib_is_empty(Scope *, Generic *[], int, int);
extern Generic *StdLib_variant(Scope *, Generic *[], int, int);
extern Generic *StdLib_variant_tag(Scope *, Generic *[], int, int);
extern Generic *StdLib_variant_values(Scope *, Generic *[], int, int);
extern Generic *StdLib_match(Scope *, Generic *[], int, int);
extern Generic *StdLib_freeze(Scope *, Generic *[], int, int);
extern Generic *StdLib_pipe(Scope *, Generic *[], int, int);
extern Generic *StdLib_partial(Scope *, Generic *[], int, int);
extern Generic *StdLib_call(Scope *, Generic *[], int, int);

/**
 * Create a new isolated capability scope with NO parent
 */
Scope *Security_createIsolatedScope(void) {
  return Scope_new(NULL);  // NULL parent = complete isolation
}

/**
 * Grant a single capability to a scope
 *
 * Returns 0 on success, -1 on unknown capability
 */
int Security_grantCapability(Scope *p_scope, const char *capability, int lineNumber) {
  if (p_scope == NULL || capability == NULL) {
    return -1;
  }

  // Dangerous I/O capabilities (explicit grant required)
  if (strcmp(capability, "print") == 0) {
    Scope_set(p_scope, "print", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_print, 0), -1);
    Scope_set(p_scope, "println", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_println, 0), -1);
    return 0;
  }

  if (strcmp(capability, "input") == 0) {
    Scope_set(p_scope, "input", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_input, 0), -1);
    return 0;
  }

  if (strcmp(capability, "files:read") == 0) {
    Scope_set(p_scope, "read_file", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_read_file, 0), -1);
    return 0;
  }

  if (strcmp(capability, "files:write") == 0) {
    Scope_set(p_scope, "write_file", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_write_file, 0), -1);
    return 0;
  }

  if (strcmp(capability, "shell") == 0) {
    Scope_set(p_scope, "shell", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_shell, 0), -1);
    return 0;
  }

  if (strcmp(capability, "events") == 0) {
    Scope_set(p_scope, "event", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_event, 0), -1);
    Scope_set(p_scope, "rows", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_rows, 0), -1);
    Scope_set(p_scope, "columns", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_columns, 0), -1);
    return 0;
  }

  if (strcmp(capability, "time") == 0) {
    Scope_set(p_scope, "wait", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_wait, 0), -1);
    Scope_set(p_scope, "time", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_time, 0), -1);
    return 0;
  }

  if (strcmp(capability, "rand") == 0) {
    Scope_set(p_scope, "random", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_random, 0), -1);
    return 0;
  }

  // Safe, pure capabilities (granted via bundles)
  if (strcmp(capability, "comparisons") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "is", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_is, 0), -1);
    Scope_set(p_scope, "less_than", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_less_than, 0), -1);
    Scope_set(p_scope, "greater_than", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_greater_than, 0), -1);
    if (strcmp(capability, "comparisons") == 0) return 0;
  }

  if (strcmp(capability, "logic") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "not", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_not, 0), -1);
    Scope_set(p_scope, "and", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_and, 0), -1);
    Scope_set(p_scope, "or", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_or, 0), -1);
    if (strcmp(capability, "logic") == 0) return 0;
  }

  if (strcmp(capability, "arithmetic") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "add", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_add, 0), -1);
    Scope_set(p_scope, "subtract", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_subtract, 0), -1);
    Scope_set(p_scope, "divide", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_divide, 0), -1);
    Scope_set(p_scope, "multiply", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_multiply, 0), -1);
    Scope_set(p_scope, "remainder", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_remainder, 0), -1);
    Scope_set(p_scope, "power", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_power, 0), -1);
    if (strcmp(capability, "arithmetic") == 0) return 0;
  }

  if (strcmp(capability, "control") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "loop", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_loop, 0), -1);
    Scope_set(p_scope, "until", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_until, 0), -1);
    Scope_set(p_scope, "if", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_if, 0), -1);
    if (strcmp(capability, "control") == 0) return 0;
  }

  if (strcmp(capability, "types") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "integer", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_integer, 0), -1);
    Scope_set(p_scope, "string", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_string, 0), -1);
    Scope_set(p_scope, "float", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_float, 0), -1);
    Scope_set(p_scope, "type", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_type, 0), -1);
    if (strcmp(capability, "types") == 0) return 0;
  }

  if (strcmp(capability, "lists") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "list", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_list, 0), -1);
    Scope_set(p_scope, "length", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_length, 0), -1);
    Scope_set(p_scope, "join", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_join, 0), -1);
    Scope_set(p_scope, "get", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_get, 0), -1);
    Scope_set(p_scope, "insert", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_insert, 0), -1);
    Scope_set(p_scope, "set", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_set, 0), -1);
    Scope_set(p_scope, "delete", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_delete, 0), -1);
    Scope_set(p_scope, "map", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_map, 0), -1);
    Scope_set(p_scope, "reduce", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_reduce, 0), -1);
    Scope_set(p_scope, "filter", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_filter, 0), -1);
    Scope_set(p_scope, "range", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_range, 0), -1);
    Scope_set(p_scope, "find", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_find, 0), -1);
    Scope_set(p_scope, "head", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_head, 0), -1);
    Scope_set(p_scope, "tail", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_tail, 0), -1);
    Scope_set(p_scope, "cons", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_cons, 0), -1);
    Scope_set(p_scope, "empty?", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_is_empty, 0), -1);
    if (strcmp(capability, "lists") == 0) return 0;
  }

  if (strcmp(capability, "variants") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "variant", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_variant, 0), -1);
    Scope_set(p_scope, "variant_tag", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_variant_tag, 0), -1);
    Scope_set(p_scope, "variant_values", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_variant_values, 0), -1);
    Scope_set(p_scope, "match", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_match, 0), -1);
    if (strcmp(capability, "variants") == 0) return 0;
  }

  if (strcmp(capability, "immutability") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "freeze", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_freeze, 0), -1);
    if (strcmp(capability, "immutability") == 0) return 0;
  }

  if (strcmp(capability, "functions") == 0 || strcmp(capability, "all") == 0) {
    Scope_set(p_scope, "pipe", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_pipe, 0), -1);
    Scope_set(p_scope, "partial", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_partial, 0), -1);
    Scope_set(p_scope, "call", Generic_new(TYPE_NATIVEFUNCTION, &StdLib_call, 0), -1);
    if (strcmp(capability, "functions") == 0) return 0;
  }

  // "all" capability handled above - return success
  if (strcmp(capability, "all") == 0) {
    return 0;
  }

  // Unknown capability
  printf("Warning @ Line %i: Unknown capability \"%s\" - ignoring.\n", lineNumber, capability);
  return -1;
}

/**
 * Seed a scope with capabilities from a list
 */
int Security_seedCapabilities(Scope *p_scope, Generic *capabilities, int lineNumber) {
  if (p_scope == NULL || capabilities == NULL) {
    return -1;
  }

  if (capabilities->type != TYPE_LIST) {
    printf(
      "Runtime Error @ Line %i: Capabilities must be a list, got %s.\n",
      lineNumber, getTypeString(capabilities->type)
    );
    return -1;
  }

  List *capList = (List *) capabilities->p_val;

  for (int i = 0; i < capList->len; i++) {
    Generic *capGen = capList->vals[i];

    // Validate each capability is a string
    if (capGen->type != TYPE_STRING) {
      printf(
        "Runtime Error @ Line %i: Capability at index %i must be a string, got %s.\n",
        lineNumber, i, getTypeString(capGen->type)
      );
      return -1;
    }

    char *cap = *((char **) capGen->p_val);
    Security_grantCapability(p_scope, cap, lineNumber);
  }

  // Always grant void (no security risk)
  Scope_set(p_scope, "void", Generic_new(TYPE_VOID, NULL, 0), -1);

  return 0;
}
