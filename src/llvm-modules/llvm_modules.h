#ifndef LLVM_MODULES_H
#define LLVM_MODULES_H

#include <llvm-c/Core.h>
#include "../ast.h"
#include "../llvm-codegen/llvm_codegen.h"

//  LLVM Module System - Industry Standard Implementation
// Provides use(), use_as(), use_with() for native compilation mode

// ============================================================================
// Module Loading - use()
// ============================================================================

/**
 * @brief Load and compile a Franz module into the current LLVM context
 *
 * Compiles the module's code and makes all top-level definitions available
 * in the provided scope. Handles circular dependency detection and caching.
 *
 * @param gen LLVM code generator context
 * @param modulePath Path to the .franz file to load
 * @param lineNumber Source line number for error reporting
 * @return 0 on success, -1 on error
 */
int LLVMModules_use(LLVMCodeGen *gen, const char *modulePath, int lineNumber);

/**
 * @brief Load multiple modules in sequence
 *
 * @param gen LLVM code generator context
 * @param modulePaths Array of module paths
 * @param moduleCount Number of modules to load
 * @param lineNumber Source line number for error reporting
 * @return 0 on success, -1 on error
 */
int LLVMModules_useMultiple(LLVMCodeGen *gen, const char **modulePaths, int moduleCount, int lineNumber);

// ============================================================================
// Namespace Support - use_as()
// ============================================================================

/**
 * @brief Load a module into a namespace prefix
 *
 * Creates prefixed symbols where all module symbols are accessible
 * with namespace_ prefix (e.g., math_sqrt, string_uppercase).
 *
 * @param gen LLVM code generator context
 * @param modulePath Path to the .franz file
 * @param namespaceName Prefix for the namespace
 * @param lineNumber Source line number for error reporting
 * @return 0 on success, -1 on error
 */
int LLVMModules_useAs(LLVMCodeGen *gen, const char *modulePath, const char *namespaceName, int lineNumber);

// ============================================================================
// Capability-Based Security - use_with()
// ============================================================================

/**
 * @brief Load a module with restricted capabilities
 *
 * Sandboxes the module by only providing access to specified capabilities.
 * Prevents untrusted code from accessing sensitive operations.
 *
 * @param gen LLVM code generator context
 * @param capabilities Array of allowed capability strings
 * @param capabilityCount Number of capabilities
 * @param modulePath Path to the .franz file
 * @param lineNumber Source line number for error reporting
 * @return 0 on success, -1 on error
 */
int LLVMModules_useWith(LLVMCodeGen *gen, const char **capabilities, int capabilityCount,
                         const char *modulePath, int lineNumber);

// ============================================================================
// Circular Dependency Detection
// ============================================================================

/**
 * @brief Initialize the module import stack for circular dependency detection
 */
void LLVMModules_initImportStack(void);

/**
 * @brief Push a module onto the import stack
 *
 * @param modulePath Path being imported
 * @param lineNumber Source line number
 * @return 0 if OK, -1 if circular dependency detected
 */
int LLVMModules_pushImport(const char *modulePath, int lineNumber);

/**
 * @brief Pop a module from the import stack after successful load
 *
 * @param modulePath Path that was imported
 */
void LLVMModules_popImport(const char *modulePath);

/**
 * @brief Print the circular dependency chain
 *
 * @param finalModule The module that caused the circular dependency
 */
void LLVMModules_printCircularChain(const char *finalModule);

/**
 * @brief Clean up the import stack
 */
void LLVMModules_freeImportStack(void);

// ============================================================================
// Module Caching
// ============================================================================

/**
 * @brief Check if a module is already compiled in cache
 *
 * @param modulePath Path to check
 * @return 1 if cached, 0 if not
 */
int LLVMModules_isCached(const char *modulePath);

/**
 * @brief Cache a compiled module's symbols
 *
 * @param modulePath Module path
 * @param symbolMap Map of symbol names to LLVM values
 */
void LLVMModules_cache(const char *modulePath, LLVMVariableMap *symbolMap);

/**
 * @brief Retrieve cached module symbols
 *
 * @param modulePath Module path
 * @return Symbol map or NULL if not cached
 */
LLVMVariableMap *LLVMModules_getCached(const char *modulePath);

/**
 * @brief Clear the module cache
 */
void LLVMModules_clearCache(void);

#endif // LLVM_MODULES_H
