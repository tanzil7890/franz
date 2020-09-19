#ifndef LLVM_STRING_OPS_H
#define LLVM_STRING_OPS_H

#include <llvm-c/Core.h>
#include "../ast.h"
#include "../llvm-codegen/llvm_codegen.h"

// ============================================================================
// LLVM String Operations - Industry-Standard Performance
// ============================================================================

/**
 * Compile (get collection index) or (get collection start end)
 *
 * LLVM-Native Implementation:
 * - Unboxes Generic* to check runtime type
 * - For strings: Pure LLVM IR substring extraction (C-level speed)
 * - For lists: Direct element access via franz_list_nth
 *
 * Performance:
 * - String operations: 100% LLVM-native, no runtime overhead
 * - List operations: Minimal overhead (just Generic* unboxing)
 * - Type checking: Runtime branch (unavoidable with Generic*)
 *
 * Examples:
 * - (get "hello" 0) → "h" (LLVM-native)
 * - (get "hello" 0 3) → "hel" (LLVM-native)
 * - (get [1,2,3] 0) → 1 (via franz_list_nth)
 */
LLVMValueRef LLVMStringOps_compileGet(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_STRING_OPS_H
