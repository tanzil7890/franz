#ifndef TYPEINFER_H
#define TYPEINFER_H

#include "types.h"
#include "ast.h"

// Type environment - maps variable names to types
typedef struct TypeEnvItem {
  char *name;
  Type *type;
  struct TypeEnvItem *next;
} TypeEnvItem;

typedef struct TypeEnv {
  TypeEnvItem *head;
  struct TypeEnv *parent;
} TypeEnv;

// Substitution for type variables
typedef struct Substitution {
  int var_id;
  Type *replacement;
  struct Substitution *next;
} Substitution;

// Inference context
typedef struct InferContext {
  TypeEnv *env;
  Substitution *subst;
  int var_counter;  // For generating fresh type variables
  int error_count;  // Number of type errors reported
} InferContext;

// Environment operations
TypeEnv *TypeEnv_new(TypeEnv *parent);
void TypeEnv_set(TypeEnv *env, char *name, Type *type);
Type *TypeEnv_get(TypeEnv *env, char *name);
void TypeEnv_free(TypeEnv *env);
void TypeEnv_print(TypeEnv *env);

// Substitution operations
Substitution *Substitution_new(int var_id, Type *replacement);
void Substitution_add(InferContext *ctx, int var_id, Type *replacement);
Type *Substitution_apply(InferContext *ctx, Type *t);
void Substitution_free(Substitution *subst);

// Fresh type variable generation
Type *fresh_type_var(InferContext *ctx);

// Unification
int unify(InferContext *ctx, Type *a, Type *b, int lineNumber);

// Main type inference function
Type *infer(AstNode *node, InferContext *ctx, int lineNumber);

// Inference context management
InferContext *InferContext_new();
void InferContext_free(InferContext *ctx);

#endif
