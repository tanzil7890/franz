#ifndef AST_H
#define AST_H
// types for ast nodes (opcodes)
enum Opcodes {
  OP_INT,
  OP_FLOAT,
  OP_STRING,
  OP_IDENTIFIER,
  OP_ASSIGNMENT,
  OP_RETURN,
  OP_STATEMENT,
  OP_APPLICATION,
  OP_FUNCTION,
  OP_SIGNATURE,   // Type signature node (sig name type)
  OP_QUALIFIED,   // Qualified name (ns.identifier)
  OP_LIST         // List literal [elem1, elem2, ...] ()
};

// node in ast
//  ARRAY-BASED AST (industry standard - Rust/OCaml/GCC/Clang style)
// Each node contains a dynamic array of children (NOT linked list)
// Provides O(1) random access to children and better cache locality
//  freeVars tracks captured variables for closures (zero-leak snapshots)
//  var_offset and var_depth for O(1) variable lookup optimization
typedef struct AstNode {
  //  Array-based children (replaces p_headChild/p_next linked list)
  struct AstNode **children;  // Dynamic array of child pointers
  int childCount;              // Number of children
  int childCapacity;           // Allocated capacity (for growth)

  enum Opcodes opcode;
  char *val;
  int lineNumber;
  char **freeVars;       //  Array of free variable names (for OP_FUNCTION)
  int freeVarsCount;     //  Number of free variables

  //  Optimization metadata for variable lookup
  int var_offset;        // Variable offset in scope array (for OP_IDENTIFIER)
  int var_depth;         // Scope depth (0 = local, 1 = parent, etc.)

  //  Mutability flag for variable declarations
  int isMutable;         // 1 if declared with 'mut', 0 otherwise (for OP_ASSIGNMENT)
} AstNode;

// prototypes
void AstNode_free(AstNode *);
char* getOpcodeString(enum Opcodes);
void AstNode_print(AstNode *, int);

//  Array-based child management (replaces appendChild)
void AstNode_addChild(AstNode *parent, AstNode *child);
void AstNode_reserveChildren(AstNode *node, int capacity);
AstNode *AstNode_getChild(AstNode *node, int index);
void AstNode_setChild(AstNode *node, int index, AstNode *child);

// Legacy function - deprecated in , kept for compatibility during migration
void AstNode_appendChild(AstNode *, AstNode **, AstNode *);

AstNode* AstNode_new(char*, enum Opcodes, int);
AstNode* AstNode_copy(AstNode *, int);

#endif