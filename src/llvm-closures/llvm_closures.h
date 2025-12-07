#ifndef LLVM_CLOSURES_H
#define LLVM_CLOSURES_H

#include <llvm-c/Core.h>
#include "../ast.h"
#include "../llvm-codegen/llvm_codegen.h"

//  LLVM Closure Support
// Implements lexical scope capture for Franz closures in native compilation
//
// Architecture (C-style closures, like Rust/C++ lambdas):
// 1. Closure Struct: { function_ptr, environment_ptr }
// 2. Environment: Heap-allocated struct with captured variables
// 3. Modified Signature: Functions with captures get env* as first parameter
//
// Example:
//   Franz: make_adder = {n -> <- {x -> <- (add n x)}}
//   LLVM:  Closure { func_ptr = @inner_func, env = {n: 5} }
//          i64 @inner_func(i8* %env, i64 %x) {
//            %n = load i64, i8* %env  ; Load captured n
//            %result = add i64 %n, %x
//            ret i64 %result
//          }

/**
 * Compiles a function that captures free variables (closure).
 *
 * Strategy:
 * 1. Analyze function AST for free variables (node->freeVars)
 * 2. Create environment struct type for captured variables
 * 3. Create function with modified signature: env* as first parameter
 * 4. Allocate environment struct and populate with captured values
 * 5. Return closure struct {function_ptr, env_ptr}
 *
 * @param gen LLVM code generator context
 * @param node OP_FUNCTION AST node (has freeVars array)
 * @return LLVMValueRef - Pointer to closure struct on heap
 */
LLVMValueRef LLVMClosures_compileClosure(LLVMCodeGen *gen, AstNode *node);

/**
 * Compiles a call to a closure (function with captured environment).
 *
 * Strategy:
 * 1. Load closure struct from variable
 * 2. Extract function pointer (field 0)
 * 3. Extract environment pointer (field 1)
 * 4. Call function with environment as first argument
 *
 * @param gen LLVM code generator context
 * @param closureValue LLVMValueRef - Closure struct pointer
 * @param args Array of compiled argument values
 * @param argCount Number of arguments
 * @param argNodes Optional array of AST nodes for each argument (may be NULL)
 * @return LLVMValueRef - Function call result
 */
LLVMValueRef LLVMClosures_callClosure(LLVMCodeGen *gen, LLVMValueRef closureValue, LLVMValueRef *args, int argCount, AstNode **argNodes);

//  Return type tags for closure type tracking
// Industry-standard approach (like Rust's dyn Trait)
//  Added CLOSURE tag to distinguish from strings
typedef enum {
  CLOSURE_RETURN_INT = 0,      // i64
  CLOSURE_RETURN_FLOAT = 1,    // double
  CLOSURE_RETURN_POINTER = 2,  // i8* (string, list, etc.)
  CLOSURE_RETURN_CLOSURE = 3,  // i8* (closure struct) -  For nested closures
  CLOSURE_RETURN_VOID = 4,     // Canonical void marker ()
  CLOSURE_RETURN_DYNAMIC = 5   // Use parameter's runtime tag (for polymorphic identity functions)
} ClosureReturnTypeTag;

/**
 * Creates the closure struct type in LLVM IR.
 *  Enhanced: Now includes return type tag for proper typing
 * Type: { i8*, i8*, i32 } = { function_pointer, environment_pointer, return_type_tag }
 *
 * This matches industry standards (Rust dyn Trait, C++ std::function):
 * - Function pointer (type-erased)
 * - Captured environment
 * - Runtime type information
 *
 * @param context LLVM context
 * @return LLVMTypeRef - Closure struct type
 */
LLVMTypeRef LLVMClosures_getClosureType(LLVMContextRef context);

/**
 * Creates an environment struct type based on captured variables.
 *
 * @param gen LLVM code generator context
 * @param freeVars Array of free variable names
 * @param freeVarsCount Number of free variables
 * @return LLVMTypeRef - Environment struct type
 */
LLVMTypeRef LLVMClosures_createEnvType(LLVMCodeGen *gen, char **freeVars, int freeVarsCount);

/**
 * Checks if a function node captures any free variables (is a closure).
 *
 * @param node OP_FUNCTION AST node
 * @return 1 if closure (has free variables), 0 if regular function
 */
int LLVMClosures_isClosure(AstNode *node);

/**
 * Determines the return type tag for a closure based on LLVM type.
 * Industry-standard fix: Correctly maps LLVM types to Franz type tags.
 *
 * - i64 → CLOSURE_RETURN_INT (0)
 * - double/float → CLOSURE_RETURN_FLOAT (1)
 * - i8* → CLOSURE_RETURN_POINTER (2)
 *
 * @param gen LLVM code generator context
 * @param returnType LLVM type of the function's return value
 * @return int - ClosureReturnTypeTag (0=INT, 1=FLOAT, 2=POINTER)
 */
int LLVMClosures_getReturnTypeTag(LLVMCodeGen *gen, LLVMTypeRef returnType);

#endif
