#ifndef STDLIB_H
#define STDLIB_H
#include "generic.h"
#include "scope.h"
#include <stdint.h>

// prototypes
Scope *newGlobal(int argc, char *argv[]);
Generic *applyFunc(Generic *, Scope *, Generic *[], int, int);

//  Runtime helpers for LLVM list literals
Generic *franz_box_int(int64_t value);
Generic *franz_box_float(double value);
Generic *franz_box_string(char *value);
Generic *franz_box_list(Generic *list);
Generic *franz_box_closure(void *closure_ptr);
Generic *franz_box_pointer_smart(void *ptr);
Generic *franz_box_param_tag(int64_t rawValue, int tag);
Generic *franz_list_new(Generic **elements, int length);

//  Unbox Generic* to get closure i64 (for nested closures)
int64_t franz_generic_to_closure_ptr(int64_t generic_i64);

//  Industry-standard list operations (Rust-like)
Generic *franz_list_head(Generic *list);
Generic *franz_list_tail(Generic *list);
Generic *franz_list_cons(Generic *elem, Generic *list);
int64_t franz_list_is_empty(Generic *list);
int64_t franz_list_length(Generic *list);
Generic *franz_list_nth(Generic *list, int64_t index);
int64_t franz_is_list(Generic *value);
void franz_print_generic(Generic *value);

// LLVM Filter
Generic *franz_llvm_filter(Generic *list, Generic *predicate, int lineNumber);

// LLVM Reduce
Generic *franz_llvm_reduce(Generic *list, Generic *callback, Generic *initial, int lineNumber);

//  Advanced file operations
Generic *franz_list_files(char *dirpath, int lineNumber);

//  Auto-Unboxing Runtime Helpers
int64_t franz_unbox_int(Generic *generic);
double franz_unbox_float(Generic *generic);
char *franz_unbox_string(Generic *generic);

#endif
