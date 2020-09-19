#ifndef GENERIC_H
#define GENERIC_H

// types for generic
enum Type {
  TYPE_INT,
  TYPE_FLOAT,
  TYPE_STRING,
  TYPE_VOID,
  TYPE_FUNCTION,
  TYPE_NATIVEFUNCTION,
  TYPE_LIST,
  TYPE_DICT,
  TYPE_NAMESPACE,
  TYPE_BYTECODE_CLOSURE,  //  Bytecode closure for compiled functions
  TYPE_REF                //  Mutable reference type
};

// generic struct
// p_val: a void pointer to the value
// type: the type of *p_val
// refCount: reference count for garbage collection
// isMutable: 1 if the value can be modified, 0 if immutable
typedef struct Generic {
  enum Type type;
  void *p_val;
  int refCount;
  int isMutable;
} Generic;

// prototypes
char* getTypeString(enum Type);
void Generic_print(Generic *);
Generic *Generic_new(enum Type, void *, int refCount);
void Generic_free(Generic *);
Generic *Generic_copy(Generic *);
int Generic_is(Generic *, Generic *);

//  Convenience constructors for bytecode compiler/VM
Generic *Generic_fromInt(int value);
Generic *Generic_fromFloat(double value);
Generic *Generic_fromString(const char *value);
Generic *Generic_fromVoid(void);

//  Reference counting helpers for bytecode VM
void Generic_retain(Generic *target);
void Generic_release(Generic *target);

#endif