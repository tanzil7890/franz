//  Test BC_CALL and BC_MAKE_CLOSURE implementation
// Compiles and runs a simple Franz program using bytecode VM

#include <stdio.h>
#include <stdlib.h>
#include "../../src/bytecode/vm.h"
#include "../../src/bytecode/compiler.h"
#include "../../src/lex.h"
#include "../../src/parse.h"
#include "../../src/stdlib.h"

int main() {
  printf("=== Testing Franz Bytecode Compiler ===\n\n");

  // Test 1: Simple arithmetic (2 + 3)
  printf("Test 1: Simple arithmetic (add 2 3)\n");
  const char *source1 = "(add 2 3)";

  VM *vm = VM_new();
  vm->globalScope = Scope_new(NULL);
  newGlobal(vm->globalScope);  // Load stdlib

  VMResult result = VM_interpret(vm, source1);

  if (result == VM_OK && vm->stackTop > 0) {
    printf("Result: ");
    Generic_print(vm->stack[vm->stackTop - 1]);
    printf("\n");
  } else {
    printf("ERROR: VM execution failed\n");
  }

  Scope_free(vm->globalScope);
  VM_free(vm);

  printf("\n=== Test Complete ===\n");
  return 0;
}
