#ifndef TYPES_H
#define TYPES_H

// Type kinds for Franz's gradual type system
typedef enum {
  TYPE_PRIM_INT,      // integer
  TYPE_PRIM_FLOAT,    // float
  TYPE_PRIM_STRING,   // string
  TYPE_PRIM_VOID,     // void
  TYPE_LIST,          // (list T)
  TYPE_FUNCTION,      // (T1 T2 -> T3)
  TYPE_VAR,           // Type variable (a, b, c) for inference
  TYPE_ANY,           // any (dynamic type)
  TYPE_UNION          // (or T1 T2)
} TypeKind;

// Type structure
typedef struct Type {
  TypeKind kind;

  union {
    // For TYPE_LIST
    struct Type *element_type;

    // For TYPE_FUNCTION
    struct {
      struct Type **param_types;
      int param_count;
      struct Type *return_type;
    } func;

    // For TYPE_VAR
    struct {
      char *var_name;
      int var_id;  // Unique ID for unification
    } var;

    // For TYPE_UNION
    struct {
      struct Type **types;
      int count;
    } union_type;
  } data;
} Type;

// Type constructors
Type *Type_int();
Type *Type_float();
Type *Type_string();
Type *Type_void();
Type *Type_any();
Type *Type_list(Type *elem);
Type *Type_function(Type **params, int count, Type *ret);
Type *Type_var(char *name, int id);
Type *Type_union(Type **types, int count);

// Type operations
int Type_equals(Type *a, Type *b);
int Type_can_unify(Type *a, Type *b);
Type *Type_copy(Type *t);
char *Type_to_string(Type *t);
void Type_free(Type *t);
void Type_print(Type *t);

// Type checking helpers
int Type_is_numeric(Type *t);
int Type_is_primitive(Type *t);

#endif
