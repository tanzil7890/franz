#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "llvm_type.h"
#include "../llvm-codegen/llvm_codegen.h"

//  LLVM Type Function Implementation
// Returns the type of a value as a string (runtime type introspection)

/**
 * Compile the type() function call
 *
 * Strategy (Industry Standard - AST-based type detection):
 * 1. Check the AST node opcode to determine type BEFORE compilation
 * 2. For literals (OP_STRING, OP_INT, OP_FLOAT, OP_LIST, OP_FUNCTION), return type directly
 * 3. For identifiers, check for special "void" keyword
 * 4. Return LLVM global string pointer with type name
 *
 * This approach matches Rust/OCaml compilers (compile-time type knowledge)
 * and avoids runtime Generic* boxing overhead for literals.
 *
 * @param gen - LLVM code generator context
 * @param node - AST node for type() function arguments
 * @return LLVMValueRef - String pointer (i8*) containing type name
 */
LLVMValueRef LLVMType_compileType(LLVMCodeGen *gen, AstNode *node) {
  if (gen->debugMode) {
    fprintf(stderr, "[TYPE] Compiling type() function call at line %d\n", node->lineNumber);
  }

  // Validate argument count
  // Note: node is the synthetic argNode containing only arguments (function name already stripped)
  if (node->childCount != 1) {
    fprintf(stderr, "ERROR: type() expects exactly 1 argument, got %d at line %d\n",
            node->childCount, node->lineNumber);
    return NULL;
  }

  // Get the argument (first child of argNode)
  AstNode *argNode = node->children[0];

  if (gen->debugMode) {
    fprintf(stderr, "[TYPE] Argument AST opcode: %d (0=int, 1=float, 2=string, 3=identifier, 8=function, 11=list)\n",
            argNode->opcode);
  }

  // INDUSTRY STANDARD APPROACH: Determine type from AST node opcode
  // This matches Rust/OCaml compilers - use compile-time type knowledge
  const char *typeString = NULL;

  switch (argNode->opcode) {
    case OP_INT:
      // Integer literal
      typeString = "integer";
      if (gen->debugMode) {
        fprintf(stderr, "[TYPE] Detected integer literal from AST\n");
      }
      break;

    case OP_FLOAT:
      // Float literal
      typeString = "float";
      if (gen->debugMode) {
        fprintf(stderr, "[TYPE] Detected float literal from AST\n");
      }
      break;

    case OP_STRING:
      // String literal
      typeString = "string";
      if (gen->debugMode) {
        fprintf(stderr, "[TYPE] Detected string literal from AST\n");
      }
      break;

    case OP_LIST:
      // List literal
      typeString = "list";
      if (gen->debugMode) {
        fprintf(stderr, "[TYPE] Detected list literal from AST\n");
      }
      break;

    case OP_FUNCTION:
      // Closure/function literal
      typeString = "closure";
      if (gen->debugMode) {
        fprintf(stderr, "[TYPE] Detected function/closure literal from AST\n");
      }
      break;

    case OP_IDENTIFIER:
      // Variable reference - check special "void" keyword first
      if (argNode->val && strcmp(argNode->val, "void") == 0) {
        typeString = "void";
        if (gen->debugMode) {
          fprintf(stderr, "[TYPE] Detected void keyword from AST\n");
        }
      } else {
        // Look up variable type from metadata (stored during assignment)
        LLVMValueRef opcodeValue = LLVMVariableMap_get(gen->typeMetadata, argNode->val);
        if (opcodeValue) {
          // Extract opcode from stored pointer (subtract 1, see assignment tracking)
          // We add 1 when storing to avoid NULL for OP_INT (opcode 0)
          int storedOpcode = (int)(intptr_t)opcodeValue - 1;

          if (gen->debugMode) {
            fprintf(stderr, "[TYPE] Found metadata for variable '%s': opcode=%d\n",
                    argNode->val, storedOpcode);
          }

          // Map opcode to type string
          // Note: OP_IDENTIFIER and OP_APPLICATION indicate computed/dynamic values
          switch (storedOpcode) {
            case OP_INT:
              typeString = "integer";
              break;
            case OP_FLOAT:
              typeString = "float";
              break;
            case OP_STRING:
              typeString = "string";
              break;
            case OP_IDENTIFIER:
              // Variable assigned from another identifier (e.g., void_var = void)
              // This is a special case - check if it's the void keyword
              // For now, we can't determine the type statically for other identifiers
              typeString = "void";  // Most common case in Franz
              break;
            case OP_LIST:
              typeString = "list";
              break;
            case OP_FUNCTION:
              typeString = "closure";
              break;
            case OP_APPLICATION:
              // Function application result - cannot determine type statically
              // This requires runtime type tags, which Franz LLVM mode doesn't have
              fprintf(stderr, "ERROR: Cannot determine type of function result variable '%s' at line %d\n",
                      argNode->val, node->lineNumber);
              fprintf(stderr, "HINT: Franz LLVM mode cannot track types for computed values\n");
              fprintf(stderr, "HINT: Use type() directly on literals instead: (type 42) instead of x=42; (type x)\n");
              return NULL;
            default:
              fprintf(stderr, "ERROR: Unknown opcode %d for variable '%s' at line %d\n",
                      storedOpcode, argNode->val, node->lineNumber);
              return NULL;
          }
        } else {
          // Variable not found in type metadata
          // FIX: Check if this is a closure parameter (has runtime type tag)
          LLVMValueRef paramTypeTag = NULL;
          if (gen->paramTypeTags) {
            paramTypeTag = LLVMVariableMap_get(gen->paramTypeTags, argNode->val);
          }

          if (paramTypeTag) {
            // This is a closure parameter - generate runtime type checking code
            fprintf(stderr, "[TYPE] Variable '%s' is closure parameter, using runtime type tag\n",
                    argNode->val);

            // Create basic blocks for switch cases
            LLVMBasicBlockRef intBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_int");
            LLVMBasicBlockRef floatBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_float");
            LLVMBasicBlockRef ptrBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_ptr");
            LLVMBasicBlockRef closureBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_closure");
            LLVMBasicBlockRef voidBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_void");
            LLVMBasicBlockRef mergeBlock = LLVMAppendBasicBlockInContext(gen->context, gen->currentFunction, "type_merge");

            // Build switch on runtime type tag
            LLVMValueRef switchInst = LLVMBuildSwitch(gen->builder, paramTypeTag, ptrBlock, 5);
            LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), 0, 0), intBlock);
            LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), 1, 0), floatBlock);
            LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), 2, 0), ptrBlock);
            LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), 3, 0), closureBlock);
            LLVMAddCase(switchInst, LLVMConstInt(LLVMInt32TypeInContext(gen->context), 4, 0), voidBlock);

            // Integer case
            LLVMPositionBuilderAtEnd(gen->builder, intBlock);
            LLVMValueRef intStr = LLVMBuildGlobalStringPtr(gen->builder, "integer", ".type_int_str");
            LLVMBuildBr(gen->builder, mergeBlock);

            // Float case
            LLVMPositionBuilderAtEnd(gen->builder, floatBlock);
            LLVMValueRef floatStr = LLVMBuildGlobalStringPtr(gen->builder, "float", ".type_float_str");
            LLVMBuildBr(gen->builder, mergeBlock);

            // Pointer case (string/list/Generic*)
            LLVMPositionBuilderAtEnd(gen->builder, ptrBlock);
            LLVMValueRef ptrStr = LLVMBuildGlobalStringPtr(gen->builder, "string", ".type_ptr_str");
            LLVMBuildBr(gen->builder, mergeBlock);

            // Closure case
            LLVMPositionBuilderAtEnd(gen->builder, closureBlock);
            LLVMValueRef closureStr = LLVMBuildGlobalStringPtr(gen->builder, "closure", ".type_closure_str");
            LLVMBuildBr(gen->builder, mergeBlock);

            // Void case
            LLVMPositionBuilderAtEnd(gen->builder, voidBlock);
            LLVMValueRef voidStr = LLVMBuildGlobalStringPtr(gen->builder, "void", ".type_void_str");
            LLVMBuildBr(gen->builder, mergeBlock);

            // Merge block - PHI node to select result
            LLVMPositionBuilderAtEnd(gen->builder, mergeBlock);
            LLVMValueRef phi = LLVMBuildPhi(gen->builder, LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0), "type_result");

            LLVMValueRef incomingValues[5] = {intStr, floatStr, ptrStr, closureStr, voidStr};
            LLVMBasicBlockRef incomingBlocks[5] = {intBlock, floatBlock, ptrBlock, closureBlock, voidBlock};
            LLVMAddIncoming(phi, incomingValues, incomingBlocks, 5);

            return phi;
          } else {
            // Not a closure parameter and not in typeMetadata - truly undefined
            fprintf(stderr, "ERROR: Variable '%s' not found (not yet defined?) at line %d\n",
                    argNode->val, node->lineNumber);
            return NULL;
          }
        }
      }
      break;

    case OP_APPLICATION:
      // Function application result - cannot determine type statically
      fprintf(stderr, "ERROR: type() on function results not yet implemented at line %d\n",
              node->lineNumber);
      fprintf(stderr, "ERROR: Use type() on literals or variables, not function calls\n");
      return NULL;

    default:
      fprintf(stderr, "ERROR: Unsupported AST node type %d for type() at line %d\n",
              argNode->opcode, node->lineNumber);
      return NULL;
  }

  if (!typeString) {
    fprintf(stderr, "ERROR: Failed to determine type at line %d\n", node->lineNumber);
    return NULL;
  }

  // Return LLVM global string constant with type name
  LLVMValueRef result = LLVMBuildGlobalStringPtr(gen->builder, typeString, ".type_str");

  if (gen->debugMode) {
    fprintf(stderr, "[TYPE] Returning type string: \"%s\"\n", typeString);
  }

  return result;  // Returns char* (i8*) pointing to type name
}
