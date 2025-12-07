#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

//  Initial capacity for children array
#define AST_INITIAL_CHILDREN_CAPACITY 4

//  Free ast (array-based)
void AstNode_free(AstNode *p_head) {
  if (p_head == NULL) return;

  // Free all children
  for (int i = 0; i < p_head->childCount; i++) {
    AstNode_free(p_head->children[i]);
  }
  free(p_head->children);

  //  Free freeVars array if present
  if (p_head->freeVars != NULL) {
    for (int i = 0; i < p_head->freeVarsCount; i++) {
      free(p_head->freeVars[i]);
    }
    free(p_head->freeVars);
  }

  // Free self
  free(p_head->val);
  free(p_head);
}

// Converts opcode to string for printing
char* getOpcodeString(enum Opcodes code) {
  switch (code) {
    case OP_INT: return "int";
    case OP_FLOAT: return "float";
    case OP_STRING: return "string";
    case OP_IDENTIFIER: return "identifier";
    case OP_ASSIGNMENT: return "assignment";
    case OP_RETURN: return "return";
    case OP_STATEMENT: return "statement";
    case OP_APPLICATION: return "application";
    case OP_FUNCTION: return "function";
    case OP_SIGNATURE: return "signature";
    case OP_QUALIFIED: return "qualified";
    case OP_LIST: return "list";  // 5
    default: return "unknown";
  }
}

//  Print ast (array-based)
void AstNode_print(AstNode *p_head, int depth) {
  if (p_head == NULL) return;

  // Print current node
  if (p_head->val == NULL) 
    printf("%i| %s\n", p_head->lineNumber, getOpcodeString(p_head->opcode));
  else 
    printf("%i| %s: %s\n", p_head->lineNumber, getOpcodeString(p_head->opcode), p_head->val);

  // Print each child
  for (int i = 0; i < p_head->childCount; i++) {
    // Create appropriate whitespace and dash
    for (int x = 0; x < depth + 1; x++) printf("   ");

    // Print child
    AstNode_print(p_head->children[i], depth + 1);
  }
}

//  Add child to parent's array
void AstNode_addChild(AstNode *parent, AstNode *child) {
  if (parent == NULL || child == NULL) return;

  // Grow array if needed
  if (parent->childCount >= parent->childCapacity) {
    int newCapacity = (parent->childCapacity == 0) ? AST_INITIAL_CHILDREN_CAPACITY : parent->childCapacity * 2;
    parent->children = realloc(parent->children, sizeof(AstNode*) * newCapacity);
    parent->childCapacity = newCapacity;
  }

  // Add child
  parent->children[parent->childCount] = child;
  parent->childCount++;
}

//  Reserve capacity for children (optimization)
void AstNode_reserveChildren(AstNode *node, int capacity) {
  if (node == NULL) return;

  if (capacity > node->childCapacity) {
    node->children = realloc(node->children, sizeof(AstNode*) * capacity);
    node->childCapacity = capacity;
  }
}

//  Get child by index
AstNode *AstNode_getChild(AstNode *node, int index) {
  if (node == NULL || index < 0 || index >= node->childCount) return NULL;
  return node->children[index];
}

//  Set child at index
void AstNode_setChild(AstNode *node, int index, AstNode *child) {
  if (node == NULL || index < 0 || index >= node->childCount) return;
  node->children[index] = child;
}

// Legacy function - kept for compatibility during migration
//  This now wraps AstNode_addChild for backward compatibility
void AstNode_appendChild(AstNode *p_head, AstNode **p_p_lastChild, AstNode *p_child) {
  AstNode_addChild(p_head, p_child);
  // p_p_lastChild is no longer used in array-based approach, but kept for API compatibility
  if (p_p_lastChild != NULL) {
    *p_p_lastChild = p_child;
  }
}

// Allocates memory for a new ast node and populates it
AstNode* AstNode_new(char* val, enum Opcodes opcode, int lineNumber) {
  AstNode *res = (AstNode *) malloc(sizeof(AstNode));
  res->opcode = opcode;
  
  //  Initialize array-based children
  res->children = NULL;
  res->childCount = 0;
  res->childCapacity = 0;
  
  res->val = NULL;

  if (val != NULL) {
    res->val = (char *) malloc(sizeof(char) * (strlen(val) + 1));
    strcpy(res->val, val);
  }

  res->lineNumber = lineNumber;

  //  Initialize free variables
  res->freeVars = NULL;
  res->freeVarsCount = 0;

  //  Initialize optimization fields
  res->var_offset = -1;  // -1 indicates unoptimized (use fallback lookup)
  res->var_depth = -1;

  //  Initialize mutability flag (default immutable)
  res->isMutable = 0;

  return res;
}

//  Creates a copy of the astnode (array-based)
// depth is the current depth of the copy (start at 0)
AstNode* AstNode_copy(AstNode *p_head, int depth) {
  if (p_head == NULL) return NULL;
  
  AstNode* p_res = AstNode_new(p_head->val, p_head->opcode, p_head->lineNumber);
  
  //  Copy children array
  if (p_head->childCount > 0) {
    p_res->children = malloc(sizeof(AstNode*) * p_head->childCapacity);
    p_res->childCount = p_head->childCount;
    p_res->childCapacity = p_head->childCapacity;

    for (int i = 0; i < p_head->childCount; i++) {
      p_res->children[i] = AstNode_copy(p_head->children[i], depth + 1);
    }
  }

  //  Copy free variables if present
  if (p_head->freeVars != NULL) {
    p_res->freeVarsCount = p_head->freeVarsCount;
    p_res->freeVars = (char **) malloc(sizeof(char *) * p_res->freeVarsCount);
    for (int i = 0; i < p_res->freeVarsCount; i++) {
      p_res->freeVars[i] = (char *) malloc(strlen(p_head->freeVars[i]) + 1);
      strcpy(p_res->freeVars[i], p_head->freeVars[i]);
    }
  }

  //  Copy optimization fields
  p_res->var_offset = p_head->var_offset;
  p_res->var_depth = p_head->var_depth;

  return p_res;
}
