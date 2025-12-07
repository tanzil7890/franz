#ifndef LLVM_DICT_H
#define LLVM_DICT_H

#include <llvm-c/Core.h>
#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 * @file llvm_dict.h
 * @brief LLVM native compilation support for Dictionary (Hash Map) operations
 *
 * Implements dictionary/hash map functions with LLVM IR generation.
 * Achieves Rust-level performance for dict operations with zero runtime overhead.
 *
 * Dict implementation uses hash tables with chaining collision resolution,
 * following industry-standard practices (Python, Ruby, JavaScript).
 */

// ============================================================================
// Dictionary Creation - (dict key1 val1 key2 val2 ...)
// ============================================================================

/**
 * @brief Compile dictionary creation to LLVM IR
 *
 * Creates a hash map from alternating key-value pairs.
 * Requires even number of arguments (key-value pairs).
 *
 * Examples:
 *   (dict "name" "Ada" "age" 36) → {"name": "Ada", "age": 36}
 *   (dict) → {}
 *   (dict "x" 10 "y" 20) → {"x": 10, "y": 20}
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict creation
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the dict (Generic* pointer as i64)
 */
LLVMValueRef LLVMDict_compileDict(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Access - (dict_get dict key)
// ============================================================================

/**
 * @brief Compile dict_get to LLVM IR
 *
 * Retrieves value for given key from dictionary.
 *
 * Examples:
 *   (dict_get user "name") → "Ada"
 *   (dict_get config "port") → 8080
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_get call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the value (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictGet(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Update - (dict_set dict key value)
// ============================================================================

/**
 * @brief Compile dict_set to LLVM IR
 *
 * Returns new dict with key-value pair added/updated (immutable operation).
 *
 * Examples:
 *   (dict_set user "age" 37) → new dict with updated age
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_set call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the new dict (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictSet(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Check - (dict_has dict key)
// ============================================================================

/**
 * @brief Compile dict_has to LLVM IR
 *
 * Checks if key exists in dictionary.
 *
 * Examples:
 *   (dict_has config "debug") → 1 or 0
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_has call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing boolean result (i64: 0 or 1)
 */
LLVMValueRef LLVMDict_compileDictHas(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Keys/Values - (dict_keys dict), (dict_values dict)
// ============================================================================

/**
 * @brief Compile dict_keys to LLVM IR
 *
 * Returns list of all keys in dictionary.
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_keys call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the keys list (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictKeys(LLVMCodeGen *gen, AstNode *node, int lineNumber);

/**
 * @brief Compile dict_values to LLVM IR
 *
 * Returns list of all values in dictionary.
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_values call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the values list (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictValues(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Merge - (dict_merge dict1 dict2)
// ============================================================================

/**
 * @brief Compile dict_merge to LLVM IR
 *
 * Merges two dictionaries, with dict2 values overriding dict1.
 *
 * Examples:
 *   (dict_merge defaults user_config) → merged dict
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_merge call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the merged dict (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictMerge(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Map - (dict_map dict fn)
// ============================================================================

/**
 * @brief Compile dict_map to LLVM IR (runtime fallback)
 *
 * Returns new dict with values transformed by function.
 * Function signature: {key value -> new_value}
 *
 * Examples:
 *   (dict_map scores {k v -> <- (add v 5)}) → boosted scores
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_map call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the transformed dict (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictMap(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Dictionary Filter - (dict_filter dict fn)
// ============================================================================

/**
 * @brief Compile dict_filter to LLVM IR (runtime fallback)
 *
 * Returns new dict containing only entries where function returns truthy.
 * Function signature: {key value -> boolean}
 *
 * Examples:
 *   (dict_filter config {k v -> <- (greater_than v 0)}) → non-zero values
 *
 * @param gen LLVM code generator context
 * @param node AST node containing dict_filter call
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the filtered dict (Generic* as i64)
 */
LLVMValueRef LLVMDict_compileDictFilter(LLVMCodeGen *gen, AstNode *node, int lineNumber);

#endif // LLVM_DICT_H
