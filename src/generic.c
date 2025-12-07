#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "generic.h"
#include "ast.h"
#include "list.h"
#include "dict.h"
#include "scope.h"
#include "closure/closure.h"  //  Closure support
#include "mutable-refs/ref.h"  //  Mutable reference support
#include "bytecode_stub.h"     //  Stub for removed bytecode closures

// print generic nicely
void Generic_print(Generic *in) {
  if (in->type == TYPE_INT) {
    printf("%i", *((int *) in->p_val));
  } else if (in->type == TYPE_FLOAT) {
    printf("%f", *((double *) in->p_val));
  } else if (in->type == TYPE_STRING) {
    printf("%s", *((char **) in->p_val));
  } else if (in->type == TYPE_VOID) {
    printf("[Void]");
  } else if (in->type == TYPE_FUNCTION) {
    printf("[Function]");
  } else if (in->type == TYPE_NATIVEFUNCTION) {
    printf("[Native Function]");
  } else if (in->type == TYPE_LIST) {
    List_print((List *) (in->p_val));
  } else if (in->type == TYPE_DICT) {
    Dict_print((Dict *) (in->p_val));
  } else if (in->type == TYPE_NAMESPACE) {
    printf("[Namespace]");
  } else if (in->type == TYPE_BYTECODE_CLOSURE) {
    printf("[Closure]");
  } else if (in->type == TYPE_REF) {
    printf("[Ref: ");
    Ref *ref = (Ref *) in->p_val;
    Generic_print(ref->value);
    printf("]");
  }
  fflush(stdout);
}

// create a new generic and return
Generic* Generic_new(enum Type type, void *p_val, int refCount) {
  Generic *res = malloc(sizeof(Generic));
  res->type = type;
  res->p_val = p_val;
  res->refCount = refCount;
  res->isMutable = 1;  // Default: mutable (backward compatible)
  return res;
}

// frees p_val of generic
void Generic_free(Generic *target) {
  #ifdef DEBUG_GENERIC_FREE
  fprintf(stderr, "[DEBUG] Generic_free: Freeing type %s (refCount was %d)\n",
          getTypeString(target->type), target->refCount);
  #endif

  // if string, free contents as well
  if (target->type == TYPE_STRING) free(*((char **) target->p_val));

  if (target->type == TYPE_LIST) {
    List_free((List *) (target->p_val)); // use list's own free function
  } else if (target->type == TYPE_DICT) {
    Dict_free((Dict *) (target->p_val)); // use dict's own free function
  } else if (target->type == TYPE_FUNCTION) {
    AstNode_free(target->p_val); // functions are in reality ast nodes, so free them with the appropriate function
  } else if (target->type == TYPE_BYTECODE_CLOSURE) {
    #ifdef DEBUG_GENERIC_FREE
    fprintf(stderr, "[DEBUG] Generic_free: Calling BytecodeClosure_free\n");
    #endif
    //  Bytecode closures removed - stub does nothing
    BytecodeClosure_free((BytecodeClosure *)target->p_val);
  } else if (target->type == TYPE_NAMESPACE) {
    Scope_free((Scope *) target->p_val); // namespaces contain Scope*, free the scope
  } else if (target->type == TYPE_REF) {
    #ifdef DEBUG_GENERIC_FREE
    fprintf(stderr, "[DEBUG] Generic_free: Calling Ref_release\n");
    #endif
    Ref_release((Ref *) target->p_val); //  mutable references have their own free function
  } else if (target->type != TYPE_NATIVEFUNCTION) {
    // dont free native functions, as their void pointers are not allocated to heap
    free(target->p_val);
  }

  target->p_val = NULL;

  // free generic itself
  free(target);
}

// returns type as a string given enum
char *getTypeString(enum Type type) {
  switch (type) {
    case TYPE_FLOAT: return "float";
    case TYPE_INT: return "integer";
    case TYPE_FUNCTION: return "function";
    case TYPE_STRING: return "string";
    case TYPE_VOID: return "void";
    case TYPE_NATIVEFUNCTION: return "native function";
    case TYPE_LIST: return "list";
    case TYPE_DICT: return "dict";
    case TYPE_NAMESPACE: return "namespace";
    case TYPE_BYTECODE_CLOSURE: return "closure";
    case TYPE_REF: return "ref";
    default: return "unknown";
  }
}

Generic *Generic_copy(Generic *target) {
  Generic *res = (Generic *) malloc(sizeof(Generic));
  res->type = target->type;
  res->refCount = 0;
  res->isMutable = target->isMutable;  // Preserve mutability

  if (res->type == TYPE_STRING) {
    res->p_val = (char **) malloc(sizeof(char *));
    *((char **) res->p_val) = malloc(sizeof(char) * (strlen(*((char **) target->p_val)) + 1));
    strcpy(*((char **) res->p_val), *((char **) target->p_val));
  } else if (res->type == TYPE_FUNCTION) {
    res->p_val = AstNode_copy(target->p_val, 0);
  } else if (res->type == TYPE_BYTECODE_CLOSURE) {
    //  Bytecode closures are shared references (shallow copy)
    res->p_val = target->p_val;
  } else if (res->type == TYPE_NATIVEFUNCTION) {
    res->p_val = target->p_val;
  } else if (res->type == TYPE_VOID) {
    res->p_val = NULL;
  } else if (res->type == TYPE_INT) {
    res->p_val = (int *) malloc(sizeof(int));
    *((int *) res->p_val) = *((int *) target->p_val);
  } else if (res->type == TYPE_FLOAT) {
    res->p_val = (double *) malloc(sizeof(double));
    *((double *) res->p_val) = *((double *) target->p_val);
  } else if (res->type == TYPE_LIST) {
    res->p_val = List_copy((List *) target->p_val);
  } else if (res->type == TYPE_DICT) {
    res->p_val = Dict_copy((Dict *) target->p_val);
  } else if (res->type == TYPE_NAMESPACE) {
    // Namespaces are shared references - don't deep copy the scope
    res->p_val = target->p_val;
  } else if (res->type == TYPE_REF) {
    //  Refs are shallow copied (both point to same Ref)
    res->p_val = Ref_copy((Ref *) target->p_val);
  }

  return res;
}

// returns 1 if a and b are the same, else returns 0
int Generic_is(Generic *a, Generic *b) {
  int res = 0;

  // case where we want numerical equality (is 1.0 1) -> 1
  if (
    (a->type == TYPE_INT || a->type == TYPE_FLOAT)
    && (b->type == TYPE_INT || b->type == TYPE_FLOAT)
  ) {
    return (
      (a->type == TYPE_FLOAT ? *((double *) a->p_val) : *((int *) a->p_val))
      == (b->type == TYPE_FLOAT ? *((double *) b->p_val) : *((int *) b->p_val))
    );
  }

  // else check types
  if (a->type == b->type) {

    // do type conversions and check data
    switch (a->type) {
      case TYPE_FLOAT:
        if (*((double *) a->p_val) == *((double *) b->p_val)) res = 1;
        break;
      case TYPE_INT:
        if (*((int *) a->p_val) == *((int *) b->p_val)) res = 1;
        break;
      case TYPE_STRING:
        if (strcmp(*((char **) a->p_val), *((char **) b->p_val)) == 0) res = 1;
        break;
      case TYPE_VOID:
        res = 1;
        break;
      case TYPE_NATIVEFUNCTION:
        if (a->p_val == b->p_val) res = 1;
        break;
      case TYPE_FUNCTION:
        if (a->p_val == b->p_val) res = 1;
        break;
      case TYPE_LIST:
        res = List_compare((List *) a->p_val, (List *) b->p_val);
        break;
      case TYPE_DICT:
        res = Dict_compare((Dict *) a->p_val, (Dict *) b->p_val);
        break;
      case TYPE_NAMESPACE:
        // Namespaces are equal if they point to the same scope
        if (a->p_val == b->p_val) res = 1;
        break;
      case TYPE_BYTECODE_CLOSURE:
        // Closures are equal if they point to the same closure object
        if (a->p_val == b->p_val) res = 1;
        break;
    }
  }

  return res;
}

//  Convenience constructors for bytecode compiler/VM
Generic *Generic_fromInt(int value) {
  int *p_int = malloc(sizeof(int));
  *p_int = value;
  return Generic_new(TYPE_INT, p_int, 1);
}

Generic *Generic_fromFloat(double value) {
  double *p_float = malloc(sizeof(double));
  *p_float = value;
  return Generic_new(TYPE_FLOAT, p_float, 1);
}

Generic *Generic_fromString(const char *value) {
  char **p_str = malloc(sizeof(char *));
  *p_str = malloc(strlen(value) + 1);
  strcpy(*p_str, value);
  return Generic_new(TYPE_STRING, p_str, 1);
}

Generic *Generic_fromVoid(void) {
  return Generic_new(TYPE_VOID, NULL, 1);
}

//  Reference counting helpers for bytecode VM
void Generic_retain(Generic *target) {
  if (target != NULL) {
    target->refCount++;
  }
}

void Generic_release(Generic *target) {
  if (target == NULL) return;

  target->refCount--;
  if (target->refCount == 0) {
    Generic_free(target);
  }
}