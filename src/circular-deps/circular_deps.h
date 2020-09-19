#ifndef CIRCULAR_DEPS_H
#define CIRCULAR_DEPS_H

/**
 * Circular Dependency Detection System for Franz
 *
 * Prevents stack overflow crashes by detecting circular imports:
 * - Tracks import stack during module loading
 * - Detects if same module is being imported recursively
 * - Provides clear error messages with full import chain
 */

/**
 * Initialize the import stack tracking system
 * Must be called at program startup
 */
void CircularDeps_init(void);

/**
 * Clean up the import stack tracking system
 * Must be called at program shutdown
 */
void CircularDeps_cleanup(void);

/**
 * Push a module onto the import stack before loading
 *
 * @param module_path - Path to the module being imported
 * @param line_number - Line number where import occurs
 * @return 0 on success, -1 if circular dependency detected
 */
int CircularDeps_push(const char *module_path, int line_number);

/**
 * Pop a module from the import stack after loading completes
 *
 * @param module_path - Path to the module that finished loading
 */
void CircularDeps_pop(const char *module_path);

/**
 * Check if a module is currently being loaded (in the stack)
 *
 * @param module_path - Path to check
 * @return 1 if in stack (circular dependency), 0 if not
 */
int CircularDeps_isLoading(const char *module_path);

/**
 * Print the current import chain (for error messages)
 * Shows the full path of imports that led to the circular dependency
 *
 * @param final_path - The path that caused the cycle
 */
void CircularDeps_printChain(const char *final_path);

/**
 * Get the current import depth (for debugging/monitoring)
 *
 * @return Number of modules currently being loaded
 */
int CircularDeps_getDepth(void);

#endif
