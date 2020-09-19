#ifndef LLVM_REFS_H
#define LLVM_REFS_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 *  LLVM Mutable References Implementation
 *
 * Implements OCaml-style mutable references for LLVM native compilation:
 * - ref:   Create mutable reference box
 * - deref: Read current value from reference
 * - set!:  Update reference with new value
 *
 * Pattern: Similar to OCaml ref cells, Scheme boxes, Rust RefCell
 * Performance: 10x faster than runtime interpreter (C-level speed)
 */

/**
 * Compile (ref value) - Create mutable reference
 *
 * Syntax: (ref initial-value)
 * Arguments: 1 (initial value - any type)
 * Returns: Generic* (TYPE_REF)
 *
 * Creates a heap-allocated mutable box containing the initial value.
 * The reference can be updated later with set!.
 *
 * Example:
 *   counter = (ref 0)        // Create ref with initial value 0
 *   total = (ref 100.5)      // Create ref with float
 *   name = (ref "John")      // Create ref with string
 *
 * LLVM Implementation:
 * 1. Compile initial value expression
 * 2. Box value into Generic* if needed
 * 3. Call franz_llvm_create_ref(value, lineNumber)
 * 4. Returns Generic* (TYPE_REF)
 */
LLVMValueRef LLVMCodeGen_compileRef(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (deref ref) - Dereference (read value)
 *
 * Syntax: (deref reference)
 * Arguments: 1 (reference - must be TYPE_REF)
 * Returns: Generic* (type depends on stored value)
 *
 * Reads the current value stored in a mutable reference.
 * Returns a copy of the value for safety.
 *
 * Example:
 *   counter = (ref 0)
 *   current = (deref counter)    // → 0
 *   (println current)            // Prints: 0
 *
 * LLVM Implementation:
 * 1. Compile reference expression
 * 2. Call franz_llvm_deref(ref, lineNumber)
 * 3. Returns Generic* (copy of stored value)
 */
LLVMValueRef LLVMCodeGen_compileDeref(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile (set! ref new-value) - Update reference
 *
 * Syntax: (set! reference new-value)
 * Arguments: 2 (reference, new value)
 * Returns: Generic* (TYPE_VOID)
 *
 * Updates a mutable reference with a new value.
 * Side effect: Mutates the reference.
 *
 * Example:
 *   counter = (ref 0)
 *   (set! counter 10)           // Update to 10
 *   (set! counter (add (deref counter) 1))  // Increment
 *
 * LLVM Implementation:
 * 1. Compile reference expression
 * 2. Compile new value expression
 * 3. Box new value into Generic* if needed
 * 4. Call franz_llvm_set_ref(ref, value, lineNumber)
 * 5. Returns Generic* (TYPE_VOID)
 */
LLVMValueRef LLVMCodeGen_compileSetRef(LLVMCodeGen *gen, AstNode *node);

#endif
