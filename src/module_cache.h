#ifndef MODULE_CACHE_H
#define MODULE_CACHE_H

#include "ast.h"
#include <stdbool.h>

#define MODULE_CACHE_SIZE 256

// Cached module entry - stores parsed AST
typedef struct CachedModule {
  char *path;           // Normalized module path
  AstNode *p_ast;       // Parsed AST (owned by cache)
  long mtime;           // File modification time for invalidation
} CachedModule;

// Global module cache
typedef struct ModuleCache {
  int index;            // Next write position (circular buffer)
  bool frozen;          // If true, don't add new entries
  CachedModule cache[MODULE_CACHE_SIZE];
} ModuleCache;

// Prototypes
void ModuleCache_init();
void ModuleCache_freeze();
CachedModule *ModuleCache_read(const char *path);
void ModuleCache_write(const char *path, AstNode *p_ast, long mtime);
void ModuleCache_free();
char *ModuleCache_normalizePath(const char *path);

#endif
