#include <stdlib.h>
#include <stdio.h>
#include "ref.h"
#include "../generic.h"

// Create a new mutable reference
Ref *Ref_new(Generic *initial_value) {
  Ref *ref = (Ref *) malloc(sizeof(Ref));
  if (ref == NULL) {
    fprintf(stderr, "Fatal Error: Failed to allocate memory for Ref\n");
    exit(1);
  }

  ref->value = initial_value;
  ref->refCount = 1;

  #ifdef DEBUG_REF
  fprintf(stderr, "[DEBUG] Ref_new: Created ref with refCount=%d\n", ref->refCount);
  #endif

  return ref;
}

// Free a mutable reference
void Ref_free(Ref *ref) {
  if (ref == NULL) return;

  #ifdef DEBUG_REF
  fprintf(stderr, "[DEBUG] Ref_free: Freeing ref (refCount was %d)\n", ref->refCount);
  #endif

  // Free the contained value
  if (ref->value != NULL) {
    ref->value->refCount--;
    if (ref->value->refCount <= 0) {
      Generic_free(ref->value);
    }
  }

  free(ref);
}

// Get current value (returns copy for safety)
Generic *Ref_get(Ref *ref) {
  if (ref == NULL || ref->value == NULL) {
    fprintf(stderr, "Runtime Error: Attempting to dereference NULL ref\n");
    exit(1);
  }

  return Generic_copy(ref->value);
}

// Set new value (replaces old value)
void Ref_set(Ref *ref, Generic *new_value) {
  if (ref == NULL) {
    fprintf(stderr, "Runtime Error: Attempting to set NULL ref\n");
    exit(1);
  }

  #ifdef DEBUG_REF
  fprintf(stderr, "[DEBUG] Ref_set: Updating ref value\n");
  #endif

  // Release old value
  if (ref->value != NULL) {
    ref->value->refCount--;
    if (ref->value->refCount <= 0) {
      Generic_free(ref->value);
    }
  }

  // Set new value
  ref->value = new_value;
  ref->value->refCount++;
}

// Copy a reference (shallow copy - both refs point to same value)
Ref *Ref_copy(Ref *ref) {
  if (ref == NULL) return NULL;

  // Increment refCount of existing ref
  ref->refCount++;

  #ifdef DEBUG_REF
  fprintf(stderr, "[DEBUG] Ref_copy: Copied ref, refCount now %d\n", ref->refCount);
  #endif

  return ref;
}

// Retain reference (increment refCount)
void Ref_retain(Ref *ref) {
  if (ref == NULL) return;
  ref->refCount++;

  #ifdef DEBUG_REF
  fprintf(stderr, "[DEBUG] Ref_retain: refCount now %d\n", ref->refCount);
  #endif
}

// Release reference (decrement refCount, free if 0)
void Ref_release(Ref *ref) {
  if (ref == NULL) return;

  ref->refCount--;

  #ifdef DEBUG_REF
  fprintf(stderr, "[DEBUG] Ref_release: refCount now %d\n", ref->refCount);
  #endif

  if (ref->refCount <= 0) {
    Ref_free(ref);
  }
}
