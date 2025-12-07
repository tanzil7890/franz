#ifndef LLVM_MAP_H
#define LLVM_MAP_H

#include <llvm-c/Core.h>
#include "../llvm-codegen/llvm_codegen.h"
#include "../ast.h"

/**
 * LLVM Map Implementation
 *
 * Compiles the map function to native code via LLVM.
 * map takes a list and a callback closure, returning a new list with transformed elements.
 *
 * Syntax: (map list callback)
 *
 * Example:
 *   numbers = [1, 2, 3, 4, 5]
 *   double = {x i -> <- (multiply x 2)}
 *   doubled = (map numbers double)
 *   // Result: [2, 4, 6, 8, 10]
 *
 * The callback receives: (element, index) -> transformed_value
 * Returns: New list with all elements transformed by the callback
 */

/**
 * Compile map function call to LLVM IR
 *
 * @param gen LLVM code generator context
 * @param node AST node for map application
 * @return LLVMValueRef Generic* pointer to result list
 */
LLVMValueRef LLVMMap_compileMap(LLVMCodeGen *gen, AstNode *node);

/**
 * Compile map2 function call to LLVM IR (multi-list map / zip-map)
 *
 * Syntax: (map2 list1 list2 callback)
 *
 * Example:
 *   list1 = [1, 2, 3]
 *   list2 = [10, 20, 30]
 *   add_both = {x y i -> <- (add x y)}
 *   sums = (map2 list1 list2 add_both)
 *   // Result: [11, 22, 33]
 *
 * The callback receives: (elem1, elem2, index) -> combined_value
 * Returns: New list with combined elements (length = min(len1, len2))
 *
 * @param gen LLVM code generator context
 * @param node AST node for map2 application
 * @return LLVMValueRef Generic* pointer to result list
 */
LLVMValueRef LLVMMap_compileMap2(LLVMCodeGen *gen, AstNode *node);

#endif // LLVM_MAP_H
