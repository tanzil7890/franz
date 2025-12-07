#ifndef LLVM_ADT_H
#define LLVM_ADT_H

#include <llvm-c/Core.h>
#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 * @file llvm_adt.h
 * @brief LLVM native compilation support for Algebraic Data Types (ADT)
 *
 * Implements variant (tagged unions) and pattern matching with LLVM IR generation.
 * Achieves Rust-level performance for ADT operations with zero runtime overhead.
 */

// ============================================================================
// Variant Construction - (variant tag values...)
// ============================================================================

/**
 * @brief Compile variant construction to LLVM IR
 *
 * Creates a tagged union as a list structure: [tag_string, [values...]]
 *
 * Examples:
 *   (variant "Some" 42)     → ["Some", [42]]
 *   (variant "None")        → ["None", []]
 *   (variant "Ok" 1 2 3)    → ["Ok", [1, 2, 3]]
 *
 * @param gen LLVM code generator context
 * @param node AST node containing variant construction
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the variant (Generic* pointer)
 */
LLVMValueRef LLVMAdt_compileVariant(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Pattern Matching - (match variant tag1 fn1 tag2 fn2 ...)
// ============================================================================

/**
 * @brief Compile pattern matching to LLVM IR
 *
 * Matches a variant's tag and calls the corresponding handler function.
 * Uses LLVM switch instruction for efficient tag comparison.
 *
 * Examples:
 *   (match some "Some" {x -> (println x)} "None" {-> (println "none")})
 *   (match result "Ok" {val -> val} "Err" {err -> 0})
 *
 * @param gen LLVM code generator context
 * @param node AST node containing match expression
 * @param lineNumber Source line number for error reporting
 * @return LLVM value representing the match result
 */
LLVMValueRef LLVMAdt_compileMatch(LLVMCodeGen *gen, AstNode *node, int lineNumber);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Extract variant tag (string) from variant structure
 *
 * @param gen LLVM code generator context
 * @param variantValue LLVM value representing the variant (Generic*)
 * @return LLVM value representing the tag string (Generic*)
 */
LLVMValueRef LLVMAdt_extractTag(LLVMCodeGen *gen, LLVMValueRef variantValue);

/**
 * @brief Extract variant values (list) from variant structure
 *
 * @param gen LLVM code generator context
 * @param variantValue LLVM value representing the variant (Generic*)
 * @return LLVM value representing the values list (Generic*)
 */
LLVMValueRef LLVMAdt_extractValues(LLVMCodeGen *gen, LLVMValueRef variantValue);

/**
 * @brief Unpack variant values into an array for parameterized handlers ()
 *
 * Extracts values from a variant's value list and converts them to i64 array
 * for passing as arguments to parameterized match handlers.
 *
 * Example: variant ["Some", [42, "hello"]] → [i64(42), i64("hello")]
 *
 * @param gen LLVM code generator context
 * @param valuesListPtr LLVM value representing the values list (Generic*)
 * @param maxValues Maximum number of values to unpack (handler parameter count)
 * @param outCount Output parameter for actual number of values unpacked
 * @return Dynamically allocated array of LLVM values (i64), caller must free()
 */
LLVMValueRef* LLVMAdt_unpackValuesToArray(LLVMCodeGen *gen, LLVMValueRef valuesListPtr, int maxValues, int *outCount);

#endif // LLVM_ADT_H
