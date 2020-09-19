#include <stdio.h>
#include <stdlib.h>
#include "typecheck.h"
#include "types.h"
#include "typeinfer.h"

// Add standard library type signatures to environment
// Following Franz philosophy: types as documentation

void typecheck_add_stdlib(TypeEnv *env) {
  // Create common type variables for polymorphic functions
  Type *a = Type_var("a", 1000);
  Type *b = Type_var("b", 1001);
  Type *c = Type_var("c", 1002);

  // Create common union types
  Type *num = Type_union((Type*[]){Type_int(), Type_float()}, 2);

  // ===== I/O Functions =====

  // print: (any ... -> void) - variadic, accepts anything
  TypeEnv_set(env, "print", Type_function((Type*[]){Type_any()}, 1, Type_void()));
  // println: (any ... -> void)
  TypeEnv_set(env, "println", Type_function((Type*[]){Type_any()}, 1, Type_void()));

  // input: (-> string)
  TypeEnv_set(env, "input", Type_function(NULL, 0, Type_string()));

  // rows: (-> integer)
  TypeEnv_set(env, "rows", Type_function(NULL, 0, Type_int()));

  // columns: (-> integer)
  TypeEnv_set(env, "columns", Type_function(NULL, 0, Type_int()));

  // read_file: (string -> string)
  TypeEnv_set(env, "read_file", Type_function((Type*[]){Type_string()}, 1, Type_string()));

  // write_file: (string string -> void)
  TypeEnv_set(env, "write_file",
    Type_function((Type*[]){Type_string(), Type_string()}, 2, Type_void()));

  // shell: (string -> string)
  TypeEnv_set(env, "shell", Type_function((Type*[]){Type_string()}, 1, Type_string()));

  // event: (-> string) or (number -> string)
  TypeEnv_set(env, "event", Type_function((Type*[]){Type_any()}, 1, Type_string()));

  // ===== Arithmetic Functions =====

  // add: (number number ... -> number)
  TypeEnv_set(env, "add", Type_function((Type*[]){num, num}, 2, Type_copy(num)));

  // subtract: (number number ... -> number)
  TypeEnv_set(env, "subtract", Type_function((Type*[]){num, num}, 2, Type_copy(num)));

  // multiply: (number number ... -> number)
  TypeEnv_set(env, "multiply", Type_function((Type*[]){num, num}, 2, Type_copy(num)));

  // divide: (number number ... -> float)
  TypeEnv_set(env, "divide", Type_function((Type*[]){num, num}, 2, Type_float()));

  // remainder: (integer integer -> integer)
  TypeEnv_set(env, "remainder",
    Type_function((Type*[]){Type_int(), Type_int()}, 2, Type_int()));

  // power: (number number -> float)
  TypeEnv_set(env, "power", Type_function((Type*[]){num, num}, 2, Type_float()));

  // random: (-> float)
  TypeEnv_set(env, "random", Type_function(NULL, 0, Type_float()));

  // ===== Comparison Functions =====

  // is: (a a -> integer) - returns 0 or 1
  TypeEnv_set(env, "is", Type_function((Type*[]){Type_copy(a), Type_copy(a)}, 2, Type_int()));

  // less_than: (number number -> integer)
  TypeEnv_set(env, "less_than", Type_function((Type*[]){num, num}, 2, Type_int()));

  // greater_than: (number number -> integer)
  TypeEnv_set(env, "greater_than", Type_function((Type*[]){num, num}, 2, Type_int()));

  // ===== Logical Functions =====

  // not: (integer -> integer)
  TypeEnv_set(env, "not", Type_function((Type*[]){Type_int()}, 1, Type_int()));

  // and: (integer integer -> integer)
  TypeEnv_set(env, "and",
    Type_function((Type*[]){Type_int(), Type_int()}, 2, Type_int()));

  // or: (integer integer -> integer)
  TypeEnv_set(env, "or",
    Type_function((Type*[]){Type_int(), Type_int()}, 2, Type_int()));

  // ===== Type Conversion =====

  // integer: (any -> integer)
  TypeEnv_set(env, "integer", Type_function((Type*[]){Type_any()}, 1, Type_int()));

  // string: (any -> string)
  TypeEnv_set(env, "string", Type_function((Type*[]){Type_any()}, 1, Type_string()));

  // float: (any -> float)
  TypeEnv_set(env, "float", Type_function((Type*[]){Type_any()}, 1, Type_float()));

  // type: (any -> string)
  TypeEnv_set(env, "type", Type_function((Type*[]){Type_any()}, 1, Type_string()));

  // ===== List Functions =====

  // list: (a ... -> (list a))
  TypeEnv_set(env, "list", Type_function((Type*[]){Type_copy(a)}, 1, Type_list(Type_copy(a))));

  // length: ((list a) -> integer) or (string -> integer)
  TypeEnv_set(env, "length", Type_function((Type*[]){Type_any()}, 1, Type_int()));

  // join: ((list a) (list a) ... -> (list a)) or (string string ... -> string)
  TypeEnv_set(env, "join", Type_function((Type*[]){Type_copy(a), Type_copy(a)}, 2, Type_copy(a)));

  // get: ((list a) integer -> a) or (string integer -> string)
  TypeEnv_set(env, "get",
    Type_function((Type*[]){Type_list(Type_copy(a)), Type_int()}, 2, Type_copy(a)));

  // insert: ((list a) a integer -> (list a))
  TypeEnv_set(env, "insert",
    Type_function((Type*[]){Type_list(Type_copy(a)), Type_copy(a), Type_int()}, 3,
                  Type_list(Type_copy(a))));

  // set: ((list a) a integer -> (list a))
  TypeEnv_set(env, "set",
    Type_function((Type*[]){Type_list(Type_copy(a)), Type_copy(a), Type_int()}, 3,
                  Type_list(Type_copy(a))));

  // delete: ((list a) integer -> (list a))
  TypeEnv_set(env, "delete",
    Type_function((Type*[]){Type_list(Type_copy(a)), Type_int()}, 2,
                  Type_list(Type_copy(a))));

  // map: ((list a) (a integer -> b) -> (list b))
  // Note: Franz map passes (item, index) to callback
  Type *map_fn = Type_function((Type*[]){Type_copy(a), Type_int()}, 2, Type_copy(b));
  TypeEnv_set(env, "map",
    Type_function((Type*[]){Type_list(Type_copy(a)), map_fn}, 2, Type_list(Type_copy(b))));

  // reduce: ((list a) (b a integer -> b) b -> b)
  Type *reduce_fn = Type_function((Type*[]){Type_copy(b), Type_copy(a), Type_int()}, 3, Type_copy(b));
  TypeEnv_set(env, "reduce",
    Type_function((Type*[]){Type_list(Type_copy(a)), reduce_fn, Type_copy(b)}, 3, Type_copy(b)));

  // filter: ((list a) (a integer -> integer) -> (list a))
  Type *filter_fn = Type_function((Type*[]){Type_copy(a), Type_int()}, 2, Type_int());
  TypeEnv_set(env, "filter",
    Type_function((Type*[]){Type_list(Type_copy(a)), filter_fn}, 2, Type_list(Type_copy(a))));

  // range: (integer -> (list integer))
  TypeEnv_set(env, "range", Type_function((Type*[]){Type_int()}, 1, Type_list(Type_int())));

  // find: ((list a) a -> integer) or (string string -> integer)
  TypeEnv_set(env, "find",
    Type_function((Type*[]){Type_list(Type_copy(a)), Type_copy(a)}, 2, Type_int()));

  // ===== Algebraic Data Types (variants) =====
  // variant: (string any... -> (list any))  [approximate as (string -> (list any))]
  TypeEnv_set(env, "variant", Type_function((Type*[]){Type_string()}, 1, Type_list(Type_any())));

  // variant_tag: ((list any) -> string)
  TypeEnv_set(env, "variant_tag", Type_function((Type*[]){Type_list(Type_any())}, 1, Type_string()));

  // variant_values: ((list any) -> (list any))
  TypeEnv_set(env, "variant_values", Type_function((Type*[]){Type_list(Type_any())}, 1, Type_list(Type_any())));

  // match: (any ... -> any) [approximate as (any -> any)]
  TypeEnv_set(env, "match", Type_function((Type*[]){Type_any()}, 1, Type_any()));

  // ===== Control Flow =====

  // if: (integer (-> a) ... -> a)
  Type *if_fn = Type_function(NULL, 0, Type_copy(a));
  TypeEnv_set(env, "if",
    Type_function((Type*[]){Type_int(), if_fn}, 2, Type_copy(a)));

  // loop: (integer (integer -> a) -> void)
  Type *loop_fn = Type_function((Type*[]){Type_int()}, 1, Type_copy(a));
  TypeEnv_set(env, "loop",
    Type_function((Type*[]){Type_int(), loop_fn}, 2, Type_void()));

  // until: (a (a integer -> a) -> a)
  Type *until_fn = Type_function((Type*[]){Type_copy(a), Type_int()}, 2, Type_copy(a));
  TypeEnv_set(env, "until",
    Type_function((Type*[]){Type_copy(a), until_fn}, 2, Type_copy(a)));

  // wait: (number -> void)
  TypeEnv_set(env, "wait", Type_function((Type*[]){num}, 1, Type_void()));

  // ===== Module System =====

  // use: (string ... (-> a) -> a)
  Type *use_fn = Type_function(NULL, 0, Type_copy(a));
  TypeEnv_set(env, "use",
    Type_function((Type*[]){Type_string(), use_fn}, 2, Type_copy(a)));

  // ===== Special Values =====

  // void constant
  TypeEnv_set(env, "void", Type_void());

  // arguments: (list string)
  TypeEnv_set(env, "arguments", Type_list(Type_string()));

  // Free temporary types
  Type_free(a);
  Type_free(b);
  Type_free(c);
  Type_free(num);
}
