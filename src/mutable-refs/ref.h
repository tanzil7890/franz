#ifndef REF_H
#define REF_H

#include "../generic.h"

// Mutable reference structure ()
// Allows controlled mutation in functional Franz
typedef struct Ref {
  Generic *value;      // The mutable value
  int refCount;        // Reference count for GC
} Ref;

// Ref operations
Ref *Ref_new(Generic *initial_value);
void Ref_free(Ref *ref);
Generic *Ref_get(Ref *ref);
void Ref_set(Ref *ref, Generic *new_value);
Ref *Ref_copy(Ref *ref);

// Reference counting
void Ref_retain(Ref *ref);
void Ref_release(Ref *ref);

#endif
