#ifndef LLVM_CONTROL_FLOW_H
#define LLVM_CONTROL_FLOW_H

#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

//  Control Flow for LLVM Native Compilation
// Implements: if statement with branch/PHI nodes

/**
 * Compile block as sequence of statements
 *
 * Used for multi-statement if/when/unless branches: {stmt1 stmt2 <- result}
 *
 * Behavior:
 * - Compiles all children as statements
 * - Returns value from last statement (explicit return or last expression)
 * - Intermediate values are discarded
 *
 * Note: This does NOT create a new function - just executes statements inline
 *
 * Public API: Used by if, when, unless, and other control flow constructs
 */
LLVMValueRef LLVMCodeGen_compileBlockAsStatements(LLVMCodeGen *gen, AstNode *blockNode);

/**
 * Compile if statement
 *
 * Syntax: (if condition then-block [else-block])
 *
 * Behavior:
 * - Evaluates condition (0 = false, non-zero = true)
 * - Branches to then-block if true, else-block if false
 * - Uses PHI node to merge results from both branches
 * - Returns value from executed branch
 * - Optional else: returns 0 if condition false and no else
 *
 * LLVM IR Strategy:
 * 1. Evaluate condition to i64
 * 2. Convert to i1 boolean (icmp ne 0)
 * 3. Create basic blocks: then, else, merge
 * 4. Branch based on condition
 * 5. Use PHI node to select result
 *
 * Example:
 *   (if 1 {<- 10} {<- 20})  → 10
 *   (if 0 {<- 10} {<- 20})  → 20
 *   (if 1 42)               → 42 (optional else)
 *   (if 0 42)               → 0  (optional else)
 *
 * Returns: LLVMValueRef - Result from the executed branch
 */
LLVMValueRef LLVMCodeGen_compileIf(LLVMCodeGen *gen, AstNode *node);

// ============================================================================
//  Control Flow Helpers
// ============================================================================

/**
 * Compile when statement
 * Execute action only if condition is true
 * Syntax: (when condition action)
 * Equivalent to: (if condition action)
 * Returns: Action result if true, 0 if false
 */
LLVMValueRef LLVMCodeGen_compileWhen(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile unless statement
 * Execute action only if condition is false
 * Syntax: (unless condition action)
 * Equivalent to: (if (not condition) action)
 * Returns: Action result if false, 0 if true
 */
LLVMValueRef LLVMCodeGen_compileUnless(LLVMCodeGen *gen, AstNode *node);

// ============================================================================
//  Cond Chains
// ============================================================================

/**
 * Compile cond chains (pattern matching conditionals)
 *
 * Syntax: (cond (test1 result1) (test2 result2) ... (else default))
 *
 * Behavior:
 * - Evaluates each test condition in order
 * - Returns result from first true condition
 * - If no conditions match, returns else clause value (or 0 if no else)
 * - Supports type promotion (INT→FLOAT) automatically
 *
 * LLVM IR Strategy:
 * - Chain of if-else blocks
 * - Each clause: test → branch to result OR next test
 * - All paths converge at merge block with PHI node
 * - Zero runtime overhead
 *
 * Example:
 *   (cond
 *     ((greater_than x 10) "big")
 *     ((greater_than x 5) "medium")
 *     (else "small"))
 *
 * Else Clause:
 * - Syntax: (else value)
 * - Must be last clause (if present)
 * - Provides default value when no conditions match
 *
 * Returns: LLVMValueRef - Result from matched clause
 */
LLVMValueRef LLVMCodeGen_compileCond(LLVMCodeGen *gen, AstNode *node);

// ============================================================================
//  Loop Control Flow
// ============================================================================

/**
 * Compile loop statement (counted iteration)
 *
 * Syntax: (loop count body)
 *
 * Behavior:
 * - Executes body `count` times
 * - Passes index i (0 to count-1) to body
 * - Body receives current iteration index
 * - Returns void (always completes all iterations)
 * - Count must be non-negative integer
 *
 * LLVM IR Strategy:
 * - Allocate counter variable (i64)
 * - Loop condition block: compare i < count
 * - Loop body block: execute body with counter
 * - Increment block: i++, branch to condition
 * - Back edge: body → condition (creates loop)
 * - Exit block: continue after loop completes
 *
 * Example:
 *   (loop 5 {i -> (println i)})
 *   // Prints: 0 1 2 3 4
 *
 * Returns: LLVMValueRef - Void (0)
 */
LLVMValueRef LLVMCodeGen_compileLoop(LLVMCodeGen *gen, AstNode *node);

// ============================================================================
//  While Loops
// ============================================================================

/**
 * Compile while loop (condition-based iteration)
 *
 * Syntax: (while condition body)
 *
 * Behavior:
 * - Evaluates condition before each iteration
 * - Executes body while condition is true (non-zero)
 * - Supports break and continue statements
 * - Returns value from break statement, or 0 if loop completes normally
 * - Infinite loop if condition is always true
 *
 * LLVM IR Strategy:
 * - Loop condition block: evaluate condition, branch
 * - Loop body block: execute body statements
 * - Loop increment block: branch back to condition (for continue)
 * - Exit block: continue after loop completes (for break)
 * - Back edge: body → condition (creates loop)
 *
 * Break/Continue Support:
 * - (break) or (break value): Exit loop immediately, return value
 * - (continue): Skip rest of iteration, jump to condition check
 *
 * Example:
 *   counter = 0
 *   (while (less_than counter 5) {
 *     (println counter)
 *     counter = (add counter 1)
 *   })
 *   // Prints: 0 1 2 3 4
 *
 * With break:
 *   counter = 0
 *   result = (while 1 {
 *     (if (is counter 5) {(break counter)} {})
 *     counter = (add counter 1)
 *   })
 *   // result = 5
 *
 * Returns: LLVMValueRef - Value from break, or 0 if completes normally
 */
LLVMValueRef LLVMCodeGen_compileWhile(LLVMCodeGen *gen, AstNode *node);

#endif
