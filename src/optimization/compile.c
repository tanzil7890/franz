#include "compile.h"
#include "../ast.h"
#include "../scope.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Global flag for optimization (can be toggled via CLI)
int g_optimization_enabled = 0;

// ============================================================================
// Binding Map Implementation
// ============================================================================

BindingMap *BindingMap_new(void) {
  BindingMap *map = (BindingMap *) malloc(sizeof(BindingMap));
  map->capacity = 16;  // Initial capacity
  map->count = 0;
  map->bindings = (VarBinding *) malloc(sizeof(VarBinding) * map->capacity);
  return map;
}

void BindingMap_free(BindingMap *map) {
  if (map == NULL) return;

  // Free all variable names
  for (int i = 0; i < map->count; i++) {
    free(map->bindings[i].name);
  }

  free(map->bindings);
  free(map);
}

void BindingMap_add(BindingMap *map, const char *name, int offset, int depth) {
  // Resize if needed
  if (map->count >= map->capacity) {
    map->capacity *= 2;
    map->bindings = (VarBinding *) realloc(map->bindings, sizeof(VarBinding) * map->capacity);
  }

  // Add new binding
  map->bindings[map->count].name = strdup(name);
  map->bindings[map->count].offset = offset;
  map->bindings[map->count].scope_depth = depth;
  map->count++;
}

VarBinding *BindingMap_lookup(BindingMap *map, const char *name) {
  for (int i = 0; i < map->count; i++) {
    if (strcmp(map->bindings[i].name, name) == 0) {
      return &map->bindings[i];
    }
  }
  return NULL;
}

void BindingMap_print(BindingMap *map) {
  printf("BindingMap (%d bindings):\n", map->count);
  for (int i = 0; i < map->count; i++) {
    printf("  [%d] %s -> offset=%d, depth=%d\n",
           i, map->bindings[i].name, map->bindings[i].offset, map->bindings[i].scope_depth);
  }
}

// ============================================================================
// Compile Environment Implementation
// ============================================================================

CompileEnv *CompileEnv_new(CompileEnv *parent) {
  CompileEnv *env = (CompileEnv *) malloc(sizeof(CompileEnv));
  env->bindings = BindingMap_new();
  env->parent = parent;
  env->depth = parent ? parent->depth + 1 : 0;
  return env;
}

void CompileEnv_free(CompileEnv *env) {
  if (env == NULL) return;
  BindingMap_free(env->bindings);
  free(env);
}

VarBinding *CompileEnv_lookup(CompileEnv *env, const char *name) {
  // Search current environment
  VarBinding *binding = BindingMap_lookup(env->bindings, name);
  if (binding != NULL) {
    return binding;
  }

  // Search parent environments
  if (env->parent != NULL) {
    return CompileEnv_lookup(env->parent, name);
  }

  return NULL;
}

void CompileEnv_print(CompileEnv *env) {
  printf("CompileEnv (depth=%d):\n", env->depth);
  BindingMap_print(env->bindings);
  if (env->parent) {
    printf("  Parent:\n");
    CompileEnv_print(env->parent);
  }
}

// ============================================================================
// AST Optimization - Variable Offset Compilation
// ============================================================================

// Helper: Count parameters in function AST
static int count_params(AstNode *fn_node) {
  if (fn_node == NULL || fn_node->opcode != OP_FUNCTION) return 0;

  // Array-based: last child is body, all others are parameters
  return (fn_node->childCount > 0) ? fn_node->childCount - 1 : 0;
}

// Main optimization function - assigns variable offsets
void compile_optimize(AstNode *root, CompileEnv *env) {
  if (root == NULL) return;
  if (!g_optimization_enabled) return;  // Skip if optimizations disabled

  switch (root->opcode) {
    case OP_IDENTIFIER: {
      // Lookup variable in compile-time environment
      VarBinding *binding = CompileEnv_lookup(env, root->val);
      if (binding != NULL) {
        // Store offset and depth in AST node for fast runtime lookup
        root->var_offset = binding->offset;
        root->var_depth = binding->scope_depth;
      }
      break;
    }

    case OP_ASSIGNMENT: {
      // Add variable to current scope's binding map
      const char *var_name = root->val;
      int offset = env->bindings->count;  // Next available offset
      BindingMap_add(env->bindings, var_name, offset, 0);  // depth=0 for local

      // Compile RHS expression (children[1])
      if (root->childCount > 1) {
        compile_optimize(root->children[1], env);
      }
      break;
    }

    case OP_FUNCTION: {
      // Create new compile-time environment for function body
      CompileEnv *func_env = CompileEnv_new(env);

      // Add parameters to function environment
      // Array-based: last child is body, all others are parameters
      int param_count = (root->childCount > 0) ? root->childCount - 1 : 0;
      for (int i = 0; i < param_count; i++) {
        BindingMap_add(func_env->bindings, root->children[i]->val, i, 0);
      }

      // Compile function body (last child)
      if (root->childCount > 0) {
        compile_optimize(root->children[root->childCount - 1], func_env);
      }

      CompileEnv_free(func_env);
      break;
    }

    case OP_APPLICATION:
    case OP_STATEMENT:
    case OP_RETURN:
      // Recursively compile children
      for (int i = 0; i < root->childCount; i++) {
        compile_optimize(root->children[i], env);
      }
      break;

    case OP_INT:
    case OP_FLOAT:
    case OP_STRING:
      // Literals - no optimization needed
      break;

    case OP_SIGNATURE:
    case OP_QUALIFIED:
      // Type annotations - recursively compile children
      for (int i = 0; i < root->childCount; i++) {
        compile_optimize(root->children[i], env);
      }
      break;

    default:
      // Unknown opcode - recursively compile children
      for (int i = 0; i < root->childCount; i++) {
        compile_optimize(root->children[i], env);
      }
      break;
  }
}
