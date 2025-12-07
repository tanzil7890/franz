#include "module_cache.h"
#include "ast.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

// Global module cache instance
static ModuleCache moduleCache = {
  .index = 0,
  .frozen = false
};

// Initialize module cache (called at startup)
void ModuleCache_init() {
  for (int i = 0; i < MODULE_CACHE_SIZE; i++) {
    moduleCache.cache[i].path = NULL;
    moduleCache.cache[i].p_ast = NULL;
    moduleCache.cache[i].mtime = 0;
  }
}

// Freeze cache - prevent new writes
void ModuleCache_freeze() {
  moduleCache.frozen = true;
}

// Normalize path (reuse normalizePath from file.c logic)
char *ModuleCache_normalizePath(const char *path) {
  // For now, simple normalization - just copy the path
  // In production, should handle ./ ../ etc like file.c does
  char *normalized = malloc(strlen(path) + 1);
  strcpy(normalized, path);
  return normalized;
}

// Get file modification time
static long get_mtime(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    return st.st_mtime;
  }
  return 0;
}

// Read cached module (returns NULL if not cached or stale)
CachedModule *ModuleCache_read(const char *path) {
  char *normalizedPath = ModuleCache_normalizePath(path);
  long current_mtime = get_mtime(path);

  // Search cache for matching path
  for (int i = 0; i < MODULE_CACHE_SIZE; i++) {
    if (moduleCache.cache[i].path != NULL &&
        strcmp(moduleCache.cache[i].path, normalizedPath) == 0) {

      // Check if file has been modified
      if (moduleCache.cache[i].mtime == current_mtime) {
        free(normalizedPath);
        return &(moduleCache.cache[i]);
      } else {
        // File modified - invalidate this cache entry
        free(moduleCache.cache[i].path);
        if (moduleCache.cache[i].p_ast != NULL) {
          AstNode_free(moduleCache.cache[i].p_ast);
        }
        moduleCache.cache[i].path = NULL;
        moduleCache.cache[i].p_ast = NULL;
        moduleCache.cache[i].mtime = 0;
        break;
      }
    }
  }

  free(normalizedPath);
  return NULL;
}

// Write module to cache
void ModuleCache_write(const char *path, AstNode *p_ast, long mtime) {
  if (moduleCache.frozen) {
    return;
  }

  int writeIndex = moduleCache.index;

  // Free existing entry at this index
  if (moduleCache.cache[writeIndex].path != NULL) {
    free(moduleCache.cache[writeIndex].path);
  }
  if (moduleCache.cache[writeIndex].p_ast != NULL) {
    AstNode_free(moduleCache.cache[writeIndex].p_ast);
  }

  // Store new entry
  moduleCache.cache[writeIndex].path = ModuleCache_normalizePath(path);
  moduleCache.cache[writeIndex].p_ast = AstNode_copy(p_ast, 1);  // Deep copy AST
  moduleCache.cache[writeIndex].mtime = mtime;

  // Advance write index (circular buffer)
  moduleCache.index = (moduleCache.index + 1) % MODULE_CACHE_SIZE;
}

// Free all cached modules
void ModuleCache_free() {
  for (int i = 0; i < MODULE_CACHE_SIZE; i++) {
    if (moduleCache.cache[i].path != NULL) {
      free(moduleCache.cache[i].path);
      moduleCache.cache[i].path = NULL;
    }
    if (moduleCache.cache[i].p_ast != NULL) {
      AstNode_free(moduleCache.cache[i].p_ast);
      moduleCache.cache[i].p_ast = NULL;
    }
    moduleCache.cache[i].mtime = 0;
  }
  moduleCache.index = 0;
}
