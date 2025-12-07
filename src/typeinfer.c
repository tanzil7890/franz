#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "typeinfer.h"
#include "types.h"
#include "ast.h"

// Type environment operations

TypeEnv *TypeEnv_new(TypeEnv *parent) {
  TypeEnv *env = (TypeEnv *) malloc(sizeof(TypeEnv));
  env->head = NULL;
  env->parent = parent;
  return env;
}

void TypeEnv_set(TypeEnv *env, char *name, Type *type) {
  // Check if name already exists
  TypeEnvItem *item = env->head;
  while (item != NULL) {
    if (strcmp(item->name, name) == 0) {
      // Update existing
      Type_free(item->type);
      item->type = Type_copy(type);
      return;
    }
    item = item->next;
  }

  // Create new item
  TypeEnvItem *new_item = (TypeEnvItem *) malloc(sizeof(TypeEnvItem));
  new_item->name = (char *) malloc(strlen(name) + 1);
  strcpy(new_item->name, name);
  new_item->type = Type_copy(type);
  new_item->next = env->head;
  env->head = new_item;
}

Type *TypeEnv_get(TypeEnv *env, char *name) {
  TypeEnvItem *item = env->head;
  while (item != NULL) {
    if (strcmp(item->name, name) == 0) {
      return item->type;
    }
    item = item->next;
  }

  // Check parent
  if (env->parent != NULL) {
    return TypeEnv_get(env->parent, name);
  }

  return NULL;
}

void TypeEnv_free(TypeEnv *env) {
  TypeEnvItem *item = env->head;
  while (item != NULL) {
    TypeEnvItem *next = item->next;
    free(item->name);
    Type_free(item->type);
    free(item);
    item = next;
  }
  free(env);
}

void TypeEnv_print(TypeEnv *env) {
  printf("Type Environment:\n");
  TypeEnvItem *item = env->head;
  while (item != NULL) {
    printf("  %s : ", item->name);
    Type_print(item->type);
    printf("\n");
    item = item->next;
  }
}

// Substitution operations

Substitution *Substitution_new(int var_id, Type *replacement) {
  Substitution *subst = (Substitution *) malloc(sizeof(Substitution));
  subst->var_id = var_id;
  subst->replacement = Type_copy(replacement);
  subst->next = NULL;
  return subst;
}

void Substitution_add(InferContext *ctx, int var_id, Type *replacement) {
  Substitution *new_subst = Substitution_new(var_id, replacement);
  new_subst->next = ctx->subst;
  ctx->subst = new_subst;
}

Type *Substitution_apply(InferContext *ctx, Type *t) {
  if (t == NULL) return NULL;

  if (t->kind == TYPE_VAR) {
    // Look up substitution
    Substitution *s = ctx->subst;
    while (s != NULL) {
      if (s->var_id == t->data.var.var_id) {
        return Substitution_apply(ctx, s->replacement);
      }
      s = s->next;
    }
    return t;
  }

  if (t->kind == TYPE_LIST) {
    Type *elem = Substitution_apply(ctx, t->data.element_type);
    return Type_list(elem);
  }

  if (t->kind == TYPE_FUNCTION) {
    Type **params = (Type **) malloc(sizeof(Type *) * t->data.func.param_count);
    for (int i = 0; i < t->data.func.param_count; i++) {
      params[i] = Substitution_apply(ctx, t->data.func.param_types[i]);
    }
    Type *ret = Substitution_apply(ctx, t->data.func.return_type);
    return Type_function(params, t->data.func.param_count, ret);
  }

  return Type_copy(t);
}

void Substitution_free(Substitution *subst) {
  while (subst != NULL) {
    Substitution *next = subst->next;
    Type_free(subst->replacement);
    free(subst);
    subst = next;
  }
}

// Fresh type variable generation

Type *fresh_type_var(InferContext *ctx) {
  char name[20];
  sprintf(name, "t%d", ctx->var_counter);
  ctx->var_counter++;
  return Type_var(name, ctx->var_counter - 1);
}

// Unification

int occurs_check(int var_id, Type *t) {
  if (t == NULL) return 0;

  if (t->kind == TYPE_VAR) {
    return t->data.var.var_id == var_id;
  }

  if (t->kind == TYPE_LIST) {
    return occurs_check(var_id, t->data.element_type);
  }

  if (t->kind == TYPE_FUNCTION) {
    for (int i = 0; i < t->data.func.param_count; i++) {
      if (occurs_check(var_id, t->data.func.param_types[i])) return 1;
    }
    return occurs_check(var_id, t->data.func.return_type);
  }

  return 0;
}

int unify(InferContext *ctx, Type *a, Type *b, int lineNumber) {
  // Apply existing substitutions
  a = Substitution_apply(ctx, a);
  b = Substitution_apply(ctx, b);

  // Same type
  if (Type_equals(a, b)) return 1;

  // Type variable cases
  if (a->kind == TYPE_VAR) {
    if (occurs_check(a->data.var.var_id, b)) {
      fprintf(stderr, "Type Error @ Line %d: Infinite type\n", lineNumber);
      ctx->error_count++;
      return 0;
    }
    Substitution_add(ctx, a->data.var.var_id, b);
    return 1;
  }

  if (b->kind == TYPE_VAR) {
    if (occurs_check(b->data.var.var_id, a)) {
      fprintf(stderr, "Type Error @ Line %d: Infinite type\n", lineNumber);
      ctx->error_count++;
      return 0;
    }
    Substitution_add(ctx, b->data.var.var_id, a);
    return 1;
  }

  // ANY type unifies with anything
  if (a->kind == TYPE_ANY || b->kind == TYPE_ANY) return 1;

  // Numeric types can unify
  if (Type_is_numeric(a) && Type_is_numeric(b)) return 1;

  // List types
  if (a->kind == TYPE_LIST && b->kind == TYPE_LIST) {
    return unify(ctx, a->data.element_type, b->data.element_type, lineNumber);
  }

  // Function types
  if (a->kind == TYPE_FUNCTION && b->kind == TYPE_FUNCTION) {
    if (a->data.func.param_count != b->data.func.param_count) {
      fprintf(stderr, "Type Error @ Line %d: Function arity mismatch\n", lineNumber);
      ctx->error_count++;
      return 0;
    }

    for (int i = 0; i < a->data.func.param_count; i++) {
      if (!unify(ctx, a->data.func.param_types[i], b->data.func.param_types[i], lineNumber)) {
        return 0;
      }
    }

    return unify(ctx, a->data.func.return_type, b->data.func.return_type, lineNumber);
  }

  // Failed to unify
  char *a_str = Type_to_string(a);
  char *b_str = Type_to_string(b);
  fprintf(stderr, "Type Error @ Line %d: Cannot unify %s with %s\n", lineNumber, a_str, b_str);
  ctx->error_count++;
  free(a_str);
  free(b_str);
  return 0;
}

// Main type inference function

Type *infer(AstNode *node, InferContext *ctx, int lineNumber) {
  if (node == NULL) return Type_void();

  switch (node->opcode) {
    case OP_INT:
      return Type_int();

    case OP_FLOAT:
      return Type_float();

    case OP_STRING:
      return Type_string();

    case OP_IDENTIFIER: {
      Type *t = TypeEnv_get(ctx->env, node->val);
      if (t == NULL) {
        // Unknown variable - generate fresh type var
        t = fresh_type_var(ctx);
        TypeEnv_set(ctx->env, node->val, t);
      }
      return Type_copy(t);
    }

    case OP_ASSIGNMENT: {
      // Get identifier name (first child)
      if (node->childCount < 2) {
        fprintf(stderr, "Type Error @ Line %d: Assignment requires identifier and value\n", lineNumber);
        ctx->error_count++;
        return Type_void();
      }
      char *name = node->children[0]->val;

      // Seed environment with a placeholder to support recursion
      Type *placeholder = fresh_type_var(ctx);
      TypeEnv_set(ctx->env, name, placeholder);

      // Infer type of value (second child)
      Type *value_type = infer(node->children[1], ctx, lineNumber);

      // Unify placeholder with inferred type and update environment
      Type *current = TypeEnv_get(ctx->env, name);
      (void) unify(ctx, current ? current : placeholder, value_type, lineNumber);
      TypeEnv_set(ctx->env, name, value_type);

      return Type_void();
    }

    case OP_FUNCTION: {
      // Create new local environment
      TypeEnv *local_env = TypeEnv_new(ctx->env);
      InferContext local_ctx = {
        .env = local_env,
        .subst = ctx->subst,
        .var_counter = ctx->var_counter
      };

      // Array-based: last child is body, all before are parameters
      int param_count = (node->childCount > 0) ? node->childCount - 1 : 0;
      Type **param_types = NULL;

      if (param_count > 0) {
        param_types = (Type **) malloc(sizeof(Type *) * param_count);
        for (int i = 0; i < param_count; i++) {
          AstNode *param = node->children[i];
          Type *param_type = fresh_type_var(&local_ctx);
          TypeEnv_set(local_env, param->val, param_type);
          param_types[i] = Type_copy(param_type);
        }
      }

      // Infer body type (last child)
      Type *body_type = Type_void();
      if (node->childCount > 0) {
        AstNode *body = node->children[node->childCount - 1];
        body_type = infer(body, &local_ctx, lineNumber);
      }

      // Update context
      ctx->var_counter = local_ctx.var_counter;
      ctx->subst = local_ctx.subst;

      // Free local environment (but not types)
      TypeEnv_free(local_env);

      // Return function type
      return Type_function(param_types, param_count, body_type);
    }

    case OP_APPLICATION: {
      if (node->childCount == 0) {
        fprintf(stderr, "Type Error @ Line %d: Empty application\n", lineNumber);
        ctx->error_count++;
        return Type_any();
      }

      // Infer function type (first child)
      Type *func_type = infer(node->children[0], ctx, lineNumber);
      func_type = Substitution_apply(ctx, func_type);

      // Count arguments (all children except first)
      int arg_count = node->childCount - 1;

      // If not a function type, try to make it one (shape type variables)
      if (func_type->kind == TYPE_VAR) {
        Type **fresh_params = NULL;
        if (arg_count > 0) {
          fresh_params = (Type **) malloc(sizeof(Type *) * arg_count);
          for (int i = 0; i < arg_count; i++) fresh_params[i] = fresh_type_var(ctx);
        }
        Type *fresh_ret = fresh_type_var(ctx);
        Type *fresh_fn = Type_function(fresh_params, arg_count, fresh_ret);
        (void) unify(ctx, func_type, fresh_fn, lineNumber);
        func_type = Substitution_apply(ctx, func_type);
      }

      // If not a function type, try to make it one
      if (func_type->kind != TYPE_FUNCTION && func_type->kind != TYPE_ANY) {
        // Try to provide more context when possible
        if (node->children[0]->opcode == OP_IDENTIFIER) {
          fprintf(stderr, "Type Error @ Line %d: Calling non-function '%s'\n", lineNumber, node->children[0]->val);
        } else {
          fprintf(stderr, "Type Error @ Line %d: Calling non-function\n", lineNumber);
        }
        ctx->error_count++;
        return Type_any();
      }

      if (func_type->kind == TYPE_ANY) {
        return Type_any();
      }

      // Identify function name if available
      const char *fname = (node->children[0]->opcode == OP_IDENTIFIER) ? node->children[0]->val : NULL;

      // Special handling for common variadic or flexible stdlib calls
      if (fname && (strcmp(fname, "print") == 0 || strcmp(fname, "println") == 0)) {
        // print: accept any number of args, all any; return void
        // Infer args but don't enforce arity or types
        for (int i = 1; i < node->childCount; i++) {
          (void) infer(node->children[i], ctx, lineNumber);
        }
        return Type_void();
      }
      if (fname && strcmp(fname, "list") == 0) {
        // list: (a ... -> (list a))
        Type *elem = fresh_type_var(ctx);
        for (int i = 1; i < node->childCount; i++) {
          Type *at = infer(node->children[i], ctx, lineNumber);
          at = Substitution_apply(ctx, at);
          (void) unify(ctx, elem, at, lineNumber);
        }
        Type *elem_final = Substitution_apply(ctx, elem);
        return Type_list(elem_final);
      }
      if (fname && strcmp(fname, "if") == 0) {
        // if: flexible arity; unify return types across closure branches when possible
        Type *ret = fresh_type_var(ctx);
        for (int i = 1; i < node->childCount; i++) {
          AstNode *a = node->children[i];
          if (i > 0 && a->opcode == OP_FUNCTION) {
            Type *ft = infer(a, ctx, lineNumber);
            if (ft && ft->kind == TYPE_FUNCTION) {
              (void) unify(ctx, ret, ft->data.func.return_type, lineNumber);
            }
          }
        }
        return Substitution_apply(ctx, ret);
      }

      if (fname && strcmp(fname, "variant") == 0) {
        // variant: (string any... -> (list any)) — accept >=1 args, return (list any)
        for (int i = 1; i < node->childCount; i++) {
          (void) infer(node->children[i], ctx, lineNumber);
        }
        return Type_list(Type_any());
      }
      if (fname && strcmp(fname, "match") == 0) {
        // match: (any tag fn ... [default]) -> unify fn return types when possible
        Type *ret = fresh_type_var(ctx);
        for (int i = 1; i < node->childCount; i++) {
          // pair: string tag then function, ignore tag, unify return
          int idx = i - 1;  // idx relative to arguments (0-based)
          if (idx % 2 == 1) {
            Type *ft = infer(node->children[i], ctx, lineNumber);
            ft = Substitution_apply(ctx, ft);
            if (ft->kind == TYPE_FUNCTION) {
              (void) unify(ctx, ret, ft->data.func.return_type, lineNumber);
            } else {
              // default or non-function; allow any
              (void) unify(ctx, ret, Type_any(), lineNumber);
            }
          }
        }
        return Substitution_apply(ctx, ret);
      }

      // Default arity check for non-variadic functions
      if (func_type->kind == TYPE_FUNCTION && arg_count != func_type->data.func.param_count) {
        if (fname) {
          fprintf(stderr, "Type Error @ Line %d: %s expects %d arguments, got %d\n",
                  lineNumber, fname, func_type->data.func.param_count, arg_count);
        } else {
          fprintf(stderr, "Type Error @ Line %d: Expected %d arguments, got %d\n",
                  lineNumber, func_type->data.func.param_count, arg_count);
        }
        ctx->error_count++;
        return Type_any();
      }

      // Infer and unify argument types
      for (int i = 0; i < arg_count; i++) {
        Type *arg_type = infer(node->children[i + 1], ctx, lineNumber);
        arg_type = Substitution_apply(ctx, arg_type);

        Type *expected = Substitution_apply(ctx, func_type->data.func.param_types[i]);

        if (!unify(ctx, arg_type, expected, lineNumber)) {
          char *got = Type_to_string(arg_type);
          char *exp = Type_to_string(expected);
          if (fname) {
            fprintf(stderr, "Type Error @ Line %d: %s argument %d type mismatch\n", lineNumber, fname, i + 1);
          } else {
            fprintf(stderr, "Type Error @ Line %d: Argument %d type mismatch\n", lineNumber, i + 1);
          }
          fprintf(stderr, "  Expected: %s\n", exp);
          fprintf(stderr, "  Got: %s\n", got);
          ctx->error_count++;
          free(got);
          free(exp);
        }
      }

      // Return result type
      Type *result = Substitution_apply(ctx, func_type->data.func.return_type);
      return Type_copy(result);
    }

    case OP_STATEMENT: {
      // Infer types of all statements
      Type *last_type = Type_void();

      for (int i = 0; i < node->childCount; i++) {
        if (last_type != NULL && last_type->kind != TYPE_PRIM_VOID) {
          Type_free(last_type);
        }
        last_type = infer(node->children[i], ctx, lineNumber);
      }

      return last_type;
    }

    case OP_RETURN: {
      if (node->childCount > 0) {
        return infer(node->children[0], ctx, lineNumber);
      }
      return Type_void();
    }

    default:
      return Type_any();
  }
}

// Inference context management

InferContext *InferContext_new() {
  InferContext *ctx = (InferContext *) malloc(sizeof(InferContext));
  ctx->env = TypeEnv_new(NULL);
  ctx->subst = NULL;
  ctx->var_counter = 0;
  ctx->error_count = 0;
  return ctx;
}

void InferContext_free(InferContext *ctx) {
  TypeEnv_free(ctx->env);
  Substitution_free(ctx->subst);
  free(ctx);
}
