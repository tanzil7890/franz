#ifndef LLVM_CODEGEN_H
#define LLVM_CODEGEN_H

#include <llvm-c/Core.h>
#include <llvm-c/ExecutionEngine.h>
#include <llvm-c/Target.h>
#include <llvm-c/Analysis.h>
#include <llvm-c/BitWriter.h>

#include "../ast.h"
#include "../scope.h"
#include "../generic.h"

//  Basic LLVM IR Generation
// Compiles Franz AST directly to LLVM IR (like Rust)

// Variable entry for hash map
typedef struct {
  char *name;
  LLVMValueRef value;
} LLVMVariable;

// Simple hash map for variables
typedef struct {
  LLVMVariable *entries;
  int count;
  int capacity;
} LLVMVariableMap;

// LLVM Code Generator Context
typedef struct {
  LLVMContextRef context;       // LLVM context
  LLVMModuleRef module;         // LLVM module
  LLVMBuilderRef builder;       // IR builder

  LLVMValueRef currentFunction; // Current function being compiled
  Scope *currentScope;          // Current Franz scope

  // Type cache (Franz types → LLVM types)
  LLVMTypeRef intType;          // i64
  LLVMTypeRef floatType;        // double
  LLVMTypeRef stringType;       // i8* (char pointer)
  LLVMTypeRef voidType;         // void
  LLVMTypeRef genericType;      // Pointer to Generic struct

  // Variable storage ()
  LLVMVariableMap *variables;   // Variable name → LLVM value mapping

  //  User-defined functions
  LLVMVariableMap *functions;   // Function name → LLVM function mapping

  //  Closure tracking (LLVM 17 opaque pointer compatibility)
  LLVMVariableMap *closures;    // Closure name → marker (non-NULL = is closure)

  //  Global symbol registry (Rust-style - external/internal linkage)
  // Tracks all globally-available symbols (built-ins, stdlib functions, user functions)
  // These are NOT captured in closures - they have external/internal linkage
  // Similar to Rust's std:: functions or OCaml's pervasives
  LLVMVariableMap *globalSymbols;  // Symbol name → marker (non-NULL = is global)

  //  Generic pointer tracking
  // Tracks which variables hold Generic* pointers (lists) vs native values (strings)
  LLVMVariableMap *genericVariables;  // Variable name → marker (non-NULL = holds Generic*)
  //  Void tracking for equality/is()
  // Marks variables known to store the canonical void value so comparisons stay type-safe
  LLVMVariableMap *voidVariables;     // Variable name → marker (non-NULL = value is void)

  //  Closure parameter runtime tag tracking
  // Maps parameter identifiers inside the current closure to their runtime type tags
  LLVMVariableMap *paramTypeTags;     // Parameter name → LLVMValueRef tag (i32)

  //  Type metadata tracking for type() function
  // Tracks the AST opcode (type) of each variable for runtime type introspection
  // Maps variable name → (void*)(intptr_t)opcode
  LLVMVariableMap *typeMetadata;  // Variable name → AST opcode (cast to void*)

  //  Return type tracking for upstream tagging optimization
  // Maps function name → return type tag (TYPE_INT, TYPE_FLOAT, TYPE_CLOSURE, etc.)
  // Used by isGenericPointerNode to avoid Generic* boxing for int/float returns
  LLVMVariableMap *returnTypeTags;  // Function name → return type tag (cast to void*)

  // Runtime function declarations ()
  LLVMValueRef printfFunc;      // printf() for output
  LLVMTypeRef printfType;       // printf function type
  LLVMValueRef putsFunc;        // puts() for println
  LLVMTypeRef putsType;         // puts function type
  LLVMValueRef fflushFunc;      // fflush(NULL) to force flush
  LLVMTypeRef fflushType;       // fflush function type

  // Type conversion runtime functions ( extension)
  LLVMValueRef atoiFunc;        // atoi() for string→int
  LLVMTypeRef atoiType;         // atoi function type
  LLVMValueRef atofFunc;        // atof() for string→float
  LLVMTypeRef atofType;         // atof function type
  LLVMValueRef snprintfFunc;    // snprintf() for int/float→string
  LLVMTypeRef snprintfType;     // snprintf function type
  LLVMValueRef mallocFunc;      // malloc() for string allocation
  LLVMTypeRef mallocType;       // malloc function type

  //  Formatting functions
  LLVMValueRef formatIntegerFunc;  // formatInteger(i64, i32) -> i8*
  LLVMTypeRef formatIntegerType;
  LLVMValueRef formatFloatFunc;    // formatFloat(double, i32) -> i8*
  LLVMTypeRef formatFloatType;

  //  String manipulation functions
  LLVMValueRef strlenFunc;      // strlen() for string length
  LLVMTypeRef strlenType;       // strlen function type
  LLVMValueRef strcpyFunc;      // strcpy() for string copy
  LLVMTypeRef strcpyType;       // strcpy function type
  LLVMValueRef strcatFunc;      // strcat() for string concatenation
  LLVMTypeRef strcatType;       // strcat function type

  //  Comparison runtime functions
  LLVMValueRef strcmpFunc;      // strcmp() for string comparison
  LLVMTypeRef strcmpType;       // strcmp function type

  //  Math functions from math.h
  LLVMValueRef fmodFunc;        // fmod() for float remainder
  LLVMTypeRef fmodType;         // fmod function type
  LLVMValueRef powFunc;         // pow() for power/exponentiation
  LLVMTypeRef powType;          // pow function type
  LLVMValueRef randFunc;        // rand() for random number generation
  LLVMTypeRef randType;         // rand function type
  LLVMValueRef srandFunc;       // srand() for seeding random
  LLVMTypeRef srandType;        // srand function type
  LLVMValueRef floorFunc;       // floor() for rounding down
  LLVMTypeRef floorType;        // floor function type
  LLVMValueRef ceilFunc;        // ceil() for rounding up
  LLVMTypeRef ceilType;         // ceil function type
  LLVMValueRef roundFunc;       // round() for rounding to nearest
  LLVMTypeRef roundType;        // round function type
  LLVMValueRef fabsFunc;        // fabs() for float absolute value
  LLVMTypeRef fabsType;         // fabs function type
  LLVMValueRef sqrtFunc;        // sqrt() for square root
  LLVMTypeRef sqrtType;         // sqrt function type

  //  I/O functions
  LLVMValueRef getcharFunc;     // getchar() for reading from stdin
  LLVMTypeRef getcharType;      // getchar function type
  LLVMValueRef reallocFunc;     // realloc() for growing buffer
  LLVMTypeRef reallocType;      // realloc function type

  //  Terminal dimension functions
  LLVMValueRef getTerminalRowsFunc;     // franz_get_terminal_rows() for terminal height
  LLVMTypeRef getTerminalRowsType;      // terminal rows function type
  LLVMValueRef getTerminalColumnsFunc;  // franz_get_terminal_columns() for terminal width
  LLVMTypeRef getTerminalColumnsType;   // terminal columns function type

  //  String repeat function
  LLVMValueRef repeatStringFunc;        // franz_repeat_string() for repeating strings
  LLVMTypeRef repeatStringType;         // repeat string function type

  //  Loop context for early exit and continue support
  LLVMBasicBlockRef loopExitBlock;  // Block to jump to for break (NULL if not in loop)
  LLVMBasicBlockRef loopIncrBlock;  // Block to jump to for continue (NULL if not in loop)
  LLVMValueRef loopReturnPtr;       // Pointer to store loop return value (NULL if not in loop)
  LLVMTypeRef loopReturnType;       // Type of loop return value (tracked for proper loading)

  //  Recursion support (forward declarations)
  const char *currentFunctionName;  // Name of function being compiled (NULL if not compiling function)

  //  Polymorphic function support
  int isPolymorphicFunction;    // 1 if current function has UNKNOWN inferred type (polymorphic)

  int debugMode;                // Print LLVM IR during compilation

  // Tail Call Optimization (TCO)
  int enableTCO;                // 1 to enable TCO, 0 to disable (controlled by --tco flag)
  int inTailPosition;           // 1 if currently compiling code in tail position (for tail call detection)
  int currentClosureReturnTag;  // Expected closure return tag (INT/FLOAT) for return conversions
} LLVMCodeGen;

//  Setup and initialization
LLVMCodeGen *LLVMCodeGen_new(const char *moduleName);
void LLVMCodeGen_free(LLVMCodeGen *gen);

// Variable map helpers ()
LLVMVariableMap *LLVMVariableMap_new();
void LLVMVariableMap_free(LLVMVariableMap *map);
void LLVMVariableMap_set(LLVMVariableMap *map, const char *name, LLVMValueRef value);
LLVMValueRef LLVMVariableMap_get(LLVMVariableMap *map, const char *name);

//  Core compilation functions
LLVMValueRef LLVMCodeGen_compileNode(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileInteger(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileFloat(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileString(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileVariable(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileAssignment(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileApplication(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileStatement(LLVMCodeGen *gen, AstNode *node);

// Stdlib function handlers ()
LLVMValueRef LLVMCodeGen_compileAdd(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileSubtract(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileMultiply(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileDivide(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compilePrintln(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compilePrint(LLVMCodeGen *gen, AstNode *node);

//  I/O function handlers
LLVMValueRef LLVMCodeGen_compileInput(LLVMCodeGen *gen, AstNode *node);

//  Terminal dimension function handlers
LLVMValueRef LLVMCodeGen_compileRows(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileColumns(LLVMCodeGen *gen, AstNode *node);

//  String repeat function handler
LLVMValueRef LLVMCodeGen_compileRepeat(LLVMCodeGen *gen, AstNode *node);

// Type conversion function handlers ( extension)
LLVMValueRef LLVMCodeGen_compileIntegerConversion(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileFloatConversion(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileStringConversion(LLVMCodeGen *gen, AstNode *node);

//  String manipulation handlers
LLVMValueRef LLVMCodeGen_compileJoin(LLVMCodeGen *gen, AstNode *node);

//  Enhanced math function handlers
LLVMValueRef LLVMCodeGen_compileRemainder(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compilePower(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileRandom(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileFloor(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileCeil(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileRound(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileAbs(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileMin(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileMax(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileSqrt(LLVMCodeGen *gen, AstNode *node);

//  Control flow (if, loop, functions)
LLVMValueRef LLVMCodeGen_compileIf(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileLoop(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileFunction(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileCall(LLVMCodeGen *gen, AstNode *node);

//  Comparison operators
LLVMValueRef LLVMCodeGen_compileIs(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileLessThan(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileGreaterThan(LLVMCodeGen *gen, AstNode *node);

//  Logical operators
LLVMValueRef LLVMCodeGen_compileNot(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileAnd(LLVMCodeGen *gen, AstNode *node);
LLVMValueRef LLVMCodeGen_compileOr(LLVMCodeGen *gen, AstNode *node);

// Output and execution
void LLVMCodeGen_dumpIR(LLVMCodeGen *gen);
int LLVMCodeGen_writeToFile(LLVMCodeGen *gen, const char *filename);
int LLVMCodeGen_compile(LLVMCodeGen *gen, AstNode *ast, Scope *globalScope);

//  Helper for semantic type checking (used by closures)
// Checks if an AST node represents a Generic* value (list, list operation, closure, etc.)
int isGenericPointerNode(LLVMCodeGen *gen, AstNode *node);

#endif
