#ifndef BYTECODE_STUB_H
#define BYTECODE_STUB_H

//  Bytecode stubs - placeholders for removed bytecode VM
// These types are still referenced in generic.c but no longer used
// In , these will be replaced with LLVM-compiled closures

typedef struct {
  void *placeholder;  // Unused - bytecode VM removed
} BytecodeClosure;

// Stub function - should never be called in 
static inline void BytecodeClosure_free(BytecodeClosure *closure) {
  (void)closure;
  // No-op - bytecode closures don't exist anymore
  // In , this will be replaced with LLVM closure cleanup
}

#endif
