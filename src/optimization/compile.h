#ifndef COMPILE_H
#define COMPILE_H

#include "../ast.h"
#include "../scope.h"

//  Variable offset compilation for O(1) lookup

// Variable binding metadata (compile-time)
typedef struct VarBinding {
  char *name;           // Variable name
  int offset;           // Index in variables array
  int scope_depth;      // 0 = local, 1 = parent, 2 = grandparent, etc.
} VarBinding;

// Binding map for tracking variable locations during compilation
typedef struct BindingMap {
  VarBinding *bindings;
  int count;
  int capacity;
} BindingMap;

// Compile-time environment (mirrors runtime Scope structure)
typedef struct CompileEnv {
  BindingMap *bindings;
  struct CompileEnv *parent;
  int depth;                    // Depth from function root (0 = local scope)
} CompileEnv;

// Prototypes for binding map operations
BindingMap *BindingMap_new(void);
void BindingMap_free(BindingMap *map);
void BindingMap_add(BindingMap *map, const char *name, int offset, int depth);
VarBinding *BindingMap_lookup(BindingMap *map, const char *name);
void BindingMap_print(BindingMap *map);

// Prototypes for compile environment operations
CompileEnv *CompileEnv_new(CompileEnv *parent);
void CompileEnv_free(CompileEnv *env);
VarBinding *CompileEnv_lookup(CompileEnv *env, const char *name);
void CompileEnv_print(CompileEnv *env);

// Main compilation function - assigns variable offsets to AST nodes
void compile_optimize(AstNode *root, CompileEnv *env);

// Global flag to enable/disable optimizations
extern int g_optimization_enabled;

#endif
