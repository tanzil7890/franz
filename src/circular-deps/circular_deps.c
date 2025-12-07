#include "circular_deps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum import depth (prevents extremely deep nesting)
#define MAX_IMPORT_DEPTH 256

// Import stack entry
typedef struct ImportStackEntry {
    char *module_path;
    int line_number;
    struct ImportStackEntry *next;
} ImportStackEntry;

// Global import stack
static ImportStackEntry *import_stack_head = NULL;
static int import_stack_depth = 0;

/**
 * Initialize the import stack tracking system
 */
void CircularDeps_init(void) {
    import_stack_head = NULL;
    import_stack_depth = 0;
}

/**
 * Clean up the import stack tracking system
 */
void CircularDeps_cleanup(void) {
    // Free all remaining entries (shouldn't happen in normal execution)
    while (import_stack_head != NULL) {
        ImportStackEntry *temp = import_stack_head;
        import_stack_head = import_stack_head->next;
        free(temp->module_path);
        free(temp);
    }
    import_stack_depth = 0;
}

/**
 * Normalize a module path for consistent comparison
 * Simple version: just use the path as-is for now
 * TODO: Could add realpath() normalization if needed
 */
static char *normalize_path(const char *path) {
    // For now, just duplicate the string
    // In the future, could use realpath() for absolute path normalization
    char *normalized = (char *)malloc(strlen(path) + 1);
    if (normalized != NULL) {
        strcpy(normalized, path);
    }
    return normalized;
}

/**
 * Check if a module is currently being loaded
 */
int CircularDeps_isLoading(const char *module_path) {
    if (module_path == NULL) {
        return 0;
    }

    char *normalized = normalize_path(module_path);
    if (normalized == NULL) {
        return 0;
    }

    ImportStackEntry *current = import_stack_head;
    while (current != NULL) {
        if (strcmp(current->module_path, normalized) == 0) {
            free(normalized);
            return 1;  // Found in stack - circular dependency!
        }
        current = current->next;
    }

    free(normalized);
    return 0;  // Not in stack - safe to load
}

/**
 * Push a module onto the import stack
 */
int CircularDeps_push(const char *module_path, int line_number) {
    if (module_path == NULL) {
        return -1;
    }

    // Check for circular dependency BEFORE pushing
    if (CircularDeps_isLoading(module_path)) {
        return -1;  // Circular dependency detected!
    }

    // Check depth limit
    if (import_stack_depth >= MAX_IMPORT_DEPTH) {
        fprintf(stderr, "Error: Maximum import depth (%d) exceeded. Possible circular dependency.\n",
                MAX_IMPORT_DEPTH);
        return -1;
    }

    // Create new stack entry
    ImportStackEntry *entry = (ImportStackEntry *)malloc(sizeof(ImportStackEntry));
    if (entry == NULL) {
        fprintf(stderr, "Error: Memory allocation failed for import stack.\n");
        return -1;
    }

    entry->module_path = normalize_path(module_path);
    if (entry->module_path == NULL) {
        free(entry);
        return -1;
    }

    entry->line_number = line_number;
    entry->next = import_stack_head;

    // Push onto stack
    import_stack_head = entry;
    import_stack_depth++;

    return 0;  // Success
}

/**
 * Pop a module from the import stack
 */
void CircularDeps_pop(const char *module_path) {
    if (module_path == NULL || import_stack_head == NULL) {
        return;
    }

    char *normalized = normalize_path(module_path);
    if (normalized == NULL) {
        return;
    }

    // Should always pop from head (LIFO), but verify for safety
    if (strcmp(import_stack_head->module_path, normalized) == 0) {
        ImportStackEntry *temp = import_stack_head;
        import_stack_head = import_stack_head->next;
        import_stack_depth--;

        free(temp->module_path);
        free(temp);
    } else {
        // Mismatch - this shouldn't happen but handle gracefully
        fprintf(stderr, "Warning: Import stack mismatch. Expected '%s' but got '%s'.\n",
                import_stack_head->module_path, normalized);
    }

    free(normalized);
}

/**
 * Print the current import chain for error messages
 */
void CircularDeps_printChain(const char *final_path) {
    fprintf(stderr, "\nCircular dependency detected:\n");
    fprintf(stderr, "=====================================\n");

    // Print stack from bottom to top (reverse order)
    // First, count entries and build array
    int count = 0;
    ImportStackEntry *current = import_stack_head;
    while (current != NULL) {
        count++;
        current = current->next;
    }

    // Print in reverse order (showing import chain from root to cycle)
    ImportStackEntry **entries = (ImportStackEntry **)malloc(sizeof(ImportStackEntry *) * count);
    if (entries != NULL) {
        current = import_stack_head;
        for (int i = count - 1; i >= 0; i--) {
            entries[i] = current;
            current = current->next;
        }

        for (int i = 0; i < count; i++) {
            fprintf(stderr, "  [%d] %s (line %d)\n",
                    i + 1, entries[i]->module_path, entries[i]->line_number);
            if (i < count - 1) {
                fprintf(stderr, "      ↓ imports\n");
            }
        }

        free(entries);
    }

    // Print the final path that would cause the cycle
    fprintf(stderr, "      ↓ tries to import\n");
    fprintf(stderr, "  [%d] %s ← CYCLE BACK TO [1]\n", count + 1, final_path);
    fprintf(stderr, "=====================================\n");
    fprintf(stderr, "Error: Cannot import '%s' - creates circular dependency.\n", final_path);
    fprintf(stderr, "Hint: Refactor to break the cycle or use lazy loading.\n");
}

/**
 * Get the current import depth
 */
int CircularDeps_getDepth(void) {
    return import_stack_depth;
}
