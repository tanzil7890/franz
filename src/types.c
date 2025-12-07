#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "types.h"

// Type constructors

Type *Type_int() {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_PRIM_INT;
  return t;
}

Type *Type_float() {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_PRIM_FLOAT;
  return t;
}

Type *Type_string() {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_PRIM_STRING;
  return t;
}

Type *Type_void() {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_PRIM_VOID;
  return t;
}

Type *Type_any() {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_ANY;
  return t;
}

Type *Type_list(Type *elem) {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_LIST;
  t->data.element_type = elem;
  return t;
}

Type *Type_function(Type **params, int count, Type *ret) {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_FUNCTION;
  // Copy the params array to heap to avoid issues with stack-allocated compound literals
  if (params != NULL && count > 0) {
    t->data.func.param_types = (Type **) malloc(sizeof(Type *) * count);
    for (int i = 0; i < count; i++) {
      t->data.func.param_types[i] = params[i];
    }
  } else {
    t->data.func.param_types = NULL;
  }
  t->data.func.param_count = count;
  t->data.func.return_type = ret;
  return t;
}

Type *Type_var(char *name, int id) {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_VAR;
  t->data.var.var_name = (char *) malloc(strlen(name) + 1);
  strcpy(t->data.var.var_name, name);
  t->data.var.var_id = id;
  return t;
}

Type *Type_union(Type **types, int count) {
  Type *t = (Type *) malloc(sizeof(Type));
  t->kind = TYPE_UNION;
  // Copy the types array to heap to avoid issues with stack-allocated compound literals
  t->data.union_type.types = (Type **) malloc(sizeof(Type *) * count);
  for (int i = 0; i < count; i++) {
    t->data.union_type.types[i] = types[i];
  }
  t->data.union_type.count = count;
  return t;
}

// Type comparison

int Type_equals(Type *a, Type *b) {
  if (a == NULL || b == NULL) return 0;
  if (a->kind != b->kind) return 0;

  switch (a->kind) {
    case TYPE_PRIM_INT:
    case TYPE_PRIM_FLOAT:
    case TYPE_PRIM_STRING:
    case TYPE_PRIM_VOID:
    case TYPE_ANY:
      return 1;

    case TYPE_LIST:
      return Type_equals(a->data.element_type, b->data.element_type);

    case TYPE_FUNCTION:
      if (a->data.func.param_count != b->data.func.param_count) return 0;
      for (int i = 0; i < a->data.func.param_count; i++) {
        if (!Type_equals(a->data.func.param_types[i], b->data.func.param_types[i]))
          return 0;
      }
      return Type_equals(a->data.func.return_type, b->data.func.return_type);

    case TYPE_VAR:
      return a->data.var.var_id == b->data.var.var_id;

    case TYPE_UNION:
      if (a->data.union_type.count != b->data.union_type.count) return 0;
      for (int i = 0; i < a->data.union_type.count; i++) {
        int found = 0;
        for (int j = 0; j < b->data.union_type.count; j++) {
          if (Type_equals(a->data.union_type.types[i], b->data.union_type.types[j])) {
            found = 1;
            break;
          }
        }
        if (!found) return 0;
      }
      return 1;

    default:
      return 0;
  }
}

// Check if two types can unify
int Type_can_unify(Type *a, Type *b) {
  if (a == NULL || b == NULL) return 0;

  // TYPE_ANY unifies with anything
  if (a->kind == TYPE_ANY || b->kind == TYPE_ANY) return 1;

  // TYPE_VAR unifies with anything
  if (a->kind == TYPE_VAR || b->kind == TYPE_VAR) return 1;

  // Same kind check
  if (a->kind != b->kind) {
    // Special case: int and float can unify (numeric types)
    if ((a->kind == TYPE_PRIM_INT && b->kind == TYPE_PRIM_FLOAT) ||
        (a->kind == TYPE_PRIM_FLOAT && b->kind == TYPE_PRIM_INT)) {
      return 1;
    }
    return 0;
  }

  switch (a->kind) {
    case TYPE_PRIM_INT:
    case TYPE_PRIM_FLOAT:
    case TYPE_PRIM_STRING:
    case TYPE_PRIM_VOID:
      return 1;

    case TYPE_LIST:
      return Type_can_unify(a->data.element_type, b->data.element_type);

    case TYPE_FUNCTION:
      if (a->data.func.param_count != b->data.func.param_count) return 0;
      for (int i = 0; i < a->data.func.param_count; i++) {
        if (!Type_can_unify(a->data.func.param_types[i], b->data.func.param_types[i]))
          return 0;
      }
      return Type_can_unify(a->data.func.return_type, b->data.func.return_type);

    case TYPE_UNION:
      // Union unifies if any type in union unifies with b
      for (int i = 0; i < a->data.union_type.count; i++) {
        if (Type_can_unify(a->data.union_type.types[i], b)) return 1;
      }
      return 0;

    default:
      return 0;
  }
}

// Copy type
Type *Type_copy(Type *t) {
  if (t == NULL) return NULL;

  switch (t->kind) {
    case TYPE_PRIM_INT:
      return Type_int();
    case TYPE_PRIM_FLOAT:
      return Type_float();
    case TYPE_PRIM_STRING:
      return Type_string();
    case TYPE_PRIM_VOID:
      return Type_void();
    case TYPE_ANY:
      return Type_any();

    case TYPE_LIST:
      return Type_list(Type_copy(t->data.element_type));

    case TYPE_FUNCTION: {
      Type **params = NULL;
      if (t->data.func.param_count > 0) {
        params = (Type **) malloc(sizeof(Type *) * t->data.func.param_count);
        for (int i = 0; i < t->data.func.param_count; i++) {
          params[i] = Type_copy(t->data.func.param_types[i]);
        }
      }
      return Type_function(params, t->data.func.param_count,
                          Type_copy(t->data.func.return_type));
    }

    case TYPE_VAR:
      return Type_var(t->data.var.var_name, t->data.var.var_id);

    case TYPE_UNION: {
      Type **types = (Type **) malloc(sizeof(Type *) * t->data.union_type.count);
      for (int i = 0; i < t->data.union_type.count; i++) {
        types[i] = Type_copy(t->data.union_type.types[i]);
      }
      return Type_union(types, t->data.union_type.count);
    }

    default:
      return Type_any();
  }
}

// Convert type to string representation
char *Type_to_string(Type *t) {
  if (t == NULL) return strdup("unknown");

  switch (t->kind) {
    case TYPE_PRIM_INT:
      return strdup("integer");
    case TYPE_PRIM_FLOAT:
      return strdup("float");
    case TYPE_PRIM_STRING:
      return strdup("string");
    case TYPE_PRIM_VOID:
      return strdup("void");
    case TYPE_ANY:
      return strdup("any");

    case TYPE_LIST: {
      char *elem_str = Type_to_string(t->data.element_type);
      char *result = (char *) malloc(strlen(elem_str) + 20);
      sprintf(result, "(list %s)", elem_str);
      free(elem_str);
      return result;
    }

    case TYPE_FUNCTION: {
      int len = 100;
      char *result = (char *) malloc(len);
      strcpy(result, "(");

      for (int i = 0; i < t->data.func.param_count; i++) {
        char *param_str = Type_to_string(t->data.func.param_types[i]);
        len += strlen(param_str) + 10;
        result = (char *) realloc(result, len);
        strcat(result, param_str);
        if (i < t->data.func.param_count - 1) strcat(result, " ");
        free(param_str);
      }

      char *ret_str = Type_to_string(t->data.func.return_type);
      len += strlen(ret_str) + 10;
      result = (char *) realloc(result, len);
      strcat(result, " -> ");
      strcat(result, ret_str);
      strcat(result, ")");
      free(ret_str);
      return result;
    }

    case TYPE_VAR: {
      char *result = (char *) malloc(strlen(t->data.var.var_name) + 10);
      sprintf(result, "%s", t->data.var.var_name);
      return result;
    }

    case TYPE_UNION: {
      int len = 100;
      char *result = (char *) malloc(len);
      strcpy(result, "(or");

      for (int i = 0; i < t->data.union_type.count; i++) {
        char *type_str = Type_to_string(t->data.union_type.types[i]);
        len += strlen(type_str) + 10;
        result = (char *) realloc(result, len);
        strcat(result, " ");
        strcat(result, type_str);
        free(type_str);
      }

      strcat(result, ")");
      return result;
    }

    default:
      return strdup("unknown");
  }
}

// Free type
void Type_free(Type *t) {
  if (t == NULL) return;

  switch (t->kind) {
    case TYPE_LIST:
      Type_free(t->data.element_type);
      break;

    case TYPE_FUNCTION:
      for (int i = 0; i < t->data.func.param_count; i++) {
        Type_free(t->data.func.param_types[i]);
      }
      free(t->data.func.param_types);
      Type_free(t->data.func.return_type);
      break;

    case TYPE_VAR:
      free(t->data.var.var_name);
      break;

    case TYPE_UNION:
      for (int i = 0; i < t->data.union_type.count; i++) {
        Type_free(t->data.union_type.types[i]);
      }
      free(t->data.union_type.types);
      break;

    default:
      break;
  }

  free(t);
}

// Print type to stdout
void Type_print(Type *t) {
  char *str = Type_to_string(t);
  printf("%s", str);
  free(str);
}

// Helper: check if type is numeric
int Type_is_numeric(Type *t) {
  if (t == NULL) return 0;
  if (t->kind == TYPE_PRIM_INT || t->kind == TYPE_PRIM_FLOAT || t->kind == TYPE_ANY) return 1;
  if (t->kind == TYPE_UNION) {
    for (int i = 0; i < t->data.union_type.count; i++) {
      if (!Type_is_numeric(t->data.union_type.types[i])) return 0;
    }
    return 1;
  }
  return 0;
}

// Helper: check if type is primitive
int Type_is_primitive(Type *t) {
  if (t == NULL) return 0;
  return (t->kind == TYPE_PRIM_INT || t->kind == TYPE_PRIM_FLOAT ||
          t->kind == TYPE_PRIM_STRING || t->kind == TYPE_PRIM_VOID);
}
