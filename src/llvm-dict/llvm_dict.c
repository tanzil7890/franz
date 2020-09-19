#include "llvm_dict.h"
#include "../llvm-codegen/llvm_codegen.h"
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Runtime Function Declarations
// ============================================================================

static void declareRuntimeDictFunctions(LLVMCodeGen *gen) {
  // franz_dict_new(capacity: i32) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_new")) {
    LLVMTypeRef dictNewParams[] = {LLVMInt32TypeInContext(gen->context)};
    LLVMTypeRef dictNewType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      dictNewParams, 1, 0
    );
    LLVMAddFunction(gen->module, "franz_dict_new", dictNewType);
  }

  // franz_dict_set_inplace(dict: ptr, key: ptr, value: ptr) -> void
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_set_inplace")) {
    LLVMTypeRef setParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)
    };
    LLVMTypeRef setType = LLVMFunctionType(LLVMVoidTypeInContext(gen->context), setParams, 3, 0);
    LLVMAddFunction(gen->module, "franz_dict_set_inplace", setType);
  }

  // franz_dict_get(dict: ptr, key: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_get")) {
    LLVMTypeRef getParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)
    };
    LLVMTypeRef getType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      getParams, 2, 0
    );
    LLVMAddFunction(gen->module, "franz_dict_get", getType);
  }

  // franz_dict_set(dict: ptr, key: ptr, value: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_set")) {
    LLVMTypeRef setParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)
    };
    LLVMTypeRef setType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      setParams, 3, 0
    );
    LLVMAddFunction(gen->module, "franz_dict_set", setType);
  }

  // franz_dict_has(dict: ptr, key: ptr) -> i32
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_has")) {
    LLVMTypeRef hasParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)
    };
    LLVMTypeRef hasType = LLVMFunctionType(LLVMInt32TypeInContext(gen->context), hasParams, 2, 0);
    LLVMAddFunction(gen->module, "franz_dict_has", hasType);
  }

  // franz_dict_keys(dict: ptr, count: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_keys")) {
    LLVMTypeRef keysParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt32TypeInContext(gen->context), 0)
    };
    LLVMTypeRef keysType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      keysParams, 2, 0
    );
    LLVMAddFunction(gen->module, "franz_dict_keys", keysType);
  }

  // franz_dict_values(dict: ptr, count: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_values")) {
    LLVMTypeRef valuesParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt32TypeInContext(gen->context), 0)
    };
    LLVMTypeRef valuesType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      valuesParams, 2, 0
    );
    LLVMAddFunction(gen->module, "franz_dict_values", valuesType);
  }

  // franz_dict_merge(dict1: ptr, dict2: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_dict_merge")) {
    LLVMTypeRef mergeParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)
    };
    LLVMTypeRef mergeType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      mergeParams, 2, 0
    );
    LLVMAddFunction(gen->module, "franz_dict_merge", mergeType);
  }

  // franz_box_dict(dict: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_box_dict")) {
    LLVMTypeRef boxParams[] = {LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)};
    LLVMTypeRef boxType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      boxParams, 1, 0
    );
    LLVMAddFunction(gen->module, "franz_box_dict", boxType);
  }

  // franz_unbox_dict(generic: ptr) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_unbox_dict")) {
    LLVMTypeRef unboxParams[] = {LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)};
    LLVMTypeRef unboxType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      unboxParams, 1, 0
    );
    LLVMAddFunction(gen->module, "franz_unbox_dict", unboxType);
  }

  // franz_box_int(value: i64) -> ptr (Generic*)
  if (!LLVMGetNamedFunction(gen->module, "franz_box_int")) {
    LLVMTypeRef boxIntParams[] = {gen->intType};
    LLVMTypeRef boxIntType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      boxIntParams, 1, 0
    );
    LLVMAddFunction(gen->module, "franz_box_int", boxIntType);
  }

  // franz_box_string(str: ptr) -> ptr (Generic*)
  if (!LLVMGetNamedFunction(gen->module, "franz_box_string")) {
    LLVMTypeRef boxStrParams[] = {LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0)};
    LLVMTypeRef boxStrType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      boxStrParams, 1, 0
    );
    LLVMAddFunction(gen->module, "franz_box_string", boxStrType);
  }

  // franz_list_new(elements: ptr, count: i64) -> ptr
  if (!LLVMGetNamedFunction(gen->module, "franz_list_new")) {
    LLVMTypeRef listParams[] = {
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      gen->intType
    };
    LLVMTypeRef listType = LLVMFunctionType(
      LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0),
      listParams, 2, 0
    );
    LLVMAddFunction(gen->module, "franz_list_new", listType);
  }
}

// ============================================================================
// (dict key1 val1 key2 val2 ...) - Dictionary Creation
// ============================================================================

LLVMValueRef LLVMDict_compileDict(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict creation at line %d\n", lineNumber);
  fprintf(stderr, "[DICT DEBUG] Number of arguments: %d\n", node->childCount - 1);

  // Validate even number of arguments (key-value pairs)
  int argCount = node->childCount - 1;  // Exclude function name
  if (argCount % 2 != 0) {
    fprintf(stderr, "ERROR: dict requires even number of arguments (key-value pairs) at line %d\n", lineNumber);
    return NULL;
  }

  int pairCount = argCount / 2;
  fprintf(stderr, "[DICT DEBUG] Creating dict with %d key-value pairs\n", pairCount);

  // Calculate initial capacity (next power of 2 >= pairCount * 2)
  int capacity = 16;
  while (capacity < pairCount * 2) {
    capacity *= 2;
  }
  fprintf(stderr, "[DICT DEBUG] Dict capacity: %d\n", capacity);

  // Create new dict
  LLVMValueRef dictNewFunc = LLVMGetNamedFunction(gen->module, "franz_dict_new");
  LLVMValueRef capacityVal = LLVMConstInt(LLVMInt32TypeInContext(gen->context), capacity, 0);
  LLVMValueRef dictPtr = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(dictNewFunc),
    dictNewFunc,
    &capacityVal,
    1,
    "dict_ptr"
  );

  fprintf(stderr, "[DICT DEBUG] Dict created, adding %d pairs\n", pairCount);

  // Add each key-value pair
  LLVMValueRef setInplaceFunc = LLVMGetNamedFunction(gen->module, "franz_dict_set_inplace");

  // Get boxing functions
  LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  for (int i = 0; i < pairCount; i++) {
    fprintf(stderr, "[DICT DEBUG] Processing pair %d/%d\n", i + 1, pairCount);

    // Compile key (at index i*2 + 1)
    AstNode *keyNode = node->children[i * 2 + 1];
    LLVMValueRef keyValue = LLVMCodeGen_compileNode(gen, keyNode);

    if (!keyValue) {
      fprintf(stderr, "ERROR: Failed to compile dict key at index %d, line %d\n", i, lineNumber);
      return NULL;
    }

    // Box key into Generic* based on type
    LLVMValueRef keyGeneric;
    if (keyNode->opcode == OP_STRING) {
      // String literal: keyValue is ptr (char*), box it
      fprintf(stderr, "[DICT DEBUG] Boxing string key\n");
      keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                  boxStringFunc, &keyValue, 1, "key_boxed");
    } else if (keyNode->opcode == OP_INT) {
      // Integer literal: keyValue is i64, box it
      fprintf(stderr, "[DICT DEBUG] Boxing integer key\n");
      keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                  boxIntFunc, &keyValue, 1, "key_boxed");
    } else {
      // Variable or other: keyValue is already Generic* as i64, convert to ptr
      fprintf(stderr, "[DICT DEBUG] Key is already boxed (i64 Generic*)\n");
      keyGeneric = LLVMBuildIntToPtr(gen->builder, keyValue, genericPtrType, "key_ptr");
    }

    // Compile value (at index i*2 + 2)
    AstNode *valueNode = node->children[i * 2 + 2];
    LLVMValueRef valueValue = LLVMCodeGen_compileNode(gen, valueNode);

    if (!valueValue) {
      fprintf(stderr, "ERROR: Failed to compile dict value at index %d, line %d\n", i, lineNumber);
      return NULL;
    }

    // Box value into Generic* based on type
    LLVMValueRef valueGeneric;
    if (valueNode->opcode == OP_STRING) {
      // String literal: valueValue is ptr (char*), box it
      fprintf(stderr, "[DICT DEBUG] Boxing string value\n");
      valueGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                    boxStringFunc, &valueValue, 1, "value_boxed");
    } else if (valueNode->opcode == OP_INT) {
      // Integer literal: valueValue is i64, box it
      fprintf(stderr, "[DICT DEBUG] Boxing integer value\n");
      valueGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                    boxIntFunc, &valueValue, 1, "value_boxed");
    } else {
      // Variable or other: valueValue is already Generic* as i64, convert to ptr
      fprintf(stderr, "[DICT DEBUG] Value is already boxed (i64 Generic*)\n");
      valueGeneric = LLVMBuildIntToPtr(gen->builder, valueValue, genericPtrType, "value_ptr");
    }

    // Call franz_dict_set_inplace(dict, key_generic, value_generic)
    LLVMValueRef setArgs[] = {dictPtr, keyGeneric, valueGeneric};
    LLVMBuildCall2(
      gen->builder,
      LLVMGlobalGetValueType(setInplaceFunc),
      setInplaceFunc,
      setArgs,
      3,
      ""
    );

    fprintf(stderr, "[DICT DEBUG] Added pair %d: key and value set\n", i + 1);
  }

  // Box dict into Generic*
  LLVMValueRef boxDictFunc = LLVMGetNamedFunction(gen->module, "franz_box_dict");
  LLVMValueRef dictGeneric = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(boxDictFunc),
    boxDictFunc,
    &dictPtr,
    1,
    "dict_generic"
  );

  // Convert Generic* to i64 for Universal Type System
  LLVMValueRef dictAsInt = LLVMBuildPtrToInt(gen->builder, dictGeneric, gen->intType, "dict_as_i64");

  fprintf(stderr, "[DICT DEBUG] Dict creation complete, returning i64\n");
  return dictAsInt;
}

// ============================================================================
// (dict_get dict key) - Get Value by Key
// ============================================================================

LLVMValueRef LLVMDict_compileDictGet(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict_get at line %d\n", lineNumber);

  // Validate argument count
  if (node->childCount != 3) {  // func + dict + key
    fprintf(stderr, "ERROR: dict_get requires 2 arguments (dict, key) at line %d\n", lineNumber);
    return NULL;
  }

  // Compile dict argument
  AstNode *dictNode = node->children[1];
  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, dictNode);

  if (!dictValue) {
    fprintf(stderr, "ERROR: Failed to compile dict argument at line %d\n", lineNumber);
    return NULL;
  }

  // Convert dict i64 to Generic* pointer
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");

  // Unbox Generic* to Dict*
  LLVMValueRef unboxDictFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_dict");
  LLVMValueRef dictPtr = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(unboxDictFunc),
    unboxDictFunc,
    &dictGenericPtr,
    1,
    "dict_ptr"
  );

  // Compile key argument
  AstNode *keyNode = node->children[2];
  LLVMValueRef keyValue = LLVMCodeGen_compileNode(gen, keyNode);

  if (!keyValue) {
    fprintf(stderr, "ERROR: Failed to compile key argument at line %d\n", lineNumber);
    return NULL;
  }

  fprintf(stderr, "[DICT DEBUG] dict_get: key node opcode=%d\n", keyNode->opcode);

  // Get boxing functions
  LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");

  // Box key into Generic* based on type (MUST match dict creation boxing)
  LLVMValueRef keyGeneric;
  if (keyNode->opcode == OP_STRING) {
    // String literal: keyValue is ptr (char*), box it
    fprintf(stderr, "[DICT DEBUG] dict_get: Boxing string key\n");
    keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                boxStringFunc, &keyValue, 1, "key_boxed");
  } else if (keyNode->opcode == OP_INT) {
    // Integer literal: keyValue is i64, box it
    fprintf(stderr, "[DICT DEBUG] dict_get: Boxing integer key\n");
    keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                boxIntFunc, &keyValue, 1, "key_boxed");
  } else {
    // Variable or other: keyValue is already Generic* as i64, convert to ptr
    fprintf(stderr, "[DICT DEBUG] dict_get: Key is already boxed (i64 Generic*)\n");
    keyGeneric = LLVMBuildIntToPtr(gen->builder, keyValue, genericPtrType, "key_ptr");
  }

  fprintf(stderr, "[DICT DEBUG] dict_get: Key boxed, calling franz_dict_get\n");

  // Call franz_dict_get(dict, key_generic) -> Generic*
  LLVMValueRef dictGetFunc = LLVMGetNamedFunction(gen->module, "franz_dict_get");
  LLVMValueRef getArgs[] = {dictPtr, keyGeneric};  // Use boxed key!
  LLVMValueRef valuePtr = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(dictGetFunc),
    dictGetFunc,
    getArgs,
    2,
    "value_ptr"
  );

  fprintf(stderr, "[DICT DEBUG] dict_get: franz_dict_get returned, converting to i64\n");

  // Convert Generic* to i64 for Universal Type System
  LLVMValueRef valueAsInt = LLVMBuildPtrToInt(gen->builder, valuePtr, gen->intType, "value_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_get complete\n");
  return valueAsInt;
}

// ============================================================================
// (dict_set dict key value) - Immutable Update
// ============================================================================

LLVMValueRef LLVMDict_compileDictSet(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict_set at line %d\n", lineNumber);

  // Validate argument count
  if (node->childCount != 4) {  // func + dict + key + value
    fprintf(stderr, "ERROR: dict_set requires 3 arguments (dict, key, value) at line %d\n", lineNumber);
    return NULL;
  }

  // Compile dict argument
  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!dictValue) {
    fprintf(stderr, "ERROR: Failed to compile dict argument at line %d\n", lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);
  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");

  LLVMValueRef unboxDictFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_dict");
  LLVMValueRef dictPtr = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(unboxDictFunc),
    unboxDictFunc,
    &dictGenericPtr,
    1,
    "dict_ptr"
  );

  // Get boxing functions
  LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");

  // Compile and box key argument
  AstNode *keyNode = node->children[2];
  LLVMValueRef keyValue = LLVMCodeGen_compileNode(gen, keyNode);
  if (!keyValue) {
    fprintf(stderr, "ERROR: Failed to compile key argument at line %d\n", lineNumber);
    return NULL;
  }

  fprintf(stderr, "[DICT DEBUG] dict_set: key node opcode=%d\n", keyNode->opcode);
  LLVMValueRef keyGeneric;
  if (keyNode->opcode == OP_STRING) {
    fprintf(stderr, "[DICT DEBUG] dict_set: Boxing string key\n");
    keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                boxStringFunc, &keyValue, 1, "key_boxed");
  } else if (keyNode->opcode == OP_INT) {
    fprintf(stderr, "[DICT DEBUG] dict_set: Boxing integer key\n");
    keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                boxIntFunc, &keyValue, 1, "key_boxed");
  } else {
    fprintf(stderr, "[DICT DEBUG] dict_set: Key already boxed\n");
    keyGeneric = LLVMBuildIntToPtr(gen->builder, keyValue, genericPtrType, "key_ptr");
  }

  // Compile and box value argument
  AstNode *valueNode = node->children[3];
  LLVMValueRef valueValue = LLVMCodeGen_compileNode(gen, valueNode);
  if (!valueValue) {
    fprintf(stderr, "ERROR: Failed to compile value argument at line %d\n", lineNumber);
    return NULL;
  }

  fprintf(stderr, "[DICT DEBUG] dict_set: value node opcode=%d\n", valueNode->opcode);
  LLVMValueRef valueGeneric;
  if (valueNode->opcode == OP_STRING) {
    fprintf(stderr, "[DICT DEBUG] dict_set: Boxing string value\n");
    valueGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                  boxStringFunc, &valueValue, 1, "value_boxed");
  } else if (valueNode->opcode == OP_INT) {
    fprintf(stderr, "[DICT DEBUG] dict_set: Boxing integer value\n");
    valueGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                  boxIntFunc, &valueValue, 1, "value_boxed");
  } else {
    fprintf(stderr, "[DICT DEBUG] dict_set: Value already boxed\n");
    valueGeneric = LLVMBuildIntToPtr(gen->builder, valueValue, genericPtrType, "value_ptr");
  }

  // Call franz_dict_set(dict, key_generic, value_generic) -> Generic*
  LLVMValueRef dictSetFunc = LLVMGetNamedFunction(gen->module, "franz_dict_set");
  LLVMValueRef setArgs[] = {dictPtr, keyGeneric, valueGeneric};
  LLVMValueRef newDictPtr = LLVMBuildCall2(
    gen->builder,
    LLVMGlobalGetValueType(dictSetFunc),
    dictSetFunc,
    setArgs,
    3,
    "new_dict_ptr"
  );

  // Convert Generic* to i64
  LLVMValueRef newDictAsInt = LLVMBuildPtrToInt(gen->builder, newDictPtr, gen->intType, "new_dict_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_set complete\n");
  return newDictAsInt;
}

// ============================================================================
// (dict_has dict key) - Check Key Existence
// ============================================================================

LLVMValueRef LLVMDict_compileDictHas(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict_has at line %d\n", lineNumber);

  if (node->childCount != 3) {
    fprintf(stderr, "ERROR: dict_has requires 2 arguments (dict, key) at line %d\n", lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Compile and unbox dict
  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!dictValue) return NULL;

  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");
  LLVMValueRef unboxDictFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_dict");
  LLVMValueRef dictPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxDictFunc), unboxDictFunc, &dictGenericPtr, 1, "dict_ptr");

  // Get boxing functions
  LLVMValueRef boxIntFunc = LLVMGetNamedFunction(gen->module, "franz_box_int");
  LLVMValueRef boxStringFunc = LLVMGetNamedFunction(gen->module, "franz_box_string");

  // Compile and box key
  AstNode *keyNode = node->children[2];
  LLVMValueRef keyValue = LLVMCodeGen_compileNode(gen, keyNode);
  if (!keyValue) return NULL;

  fprintf(stderr, "[DICT DEBUG] dict_has: key node opcode=%d\n", keyNode->opcode);
  LLVMValueRef keyGeneric;
  if (keyNode->opcode == OP_STRING) {
    fprintf(stderr, "[DICT DEBUG] dict_has: Boxing string key\n");
    keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxStringFunc),
                                boxStringFunc, &keyValue, 1, "key_boxed");
  } else if (keyNode->opcode == OP_INT) {
    fprintf(stderr, "[DICT DEBUG] dict_has: Boxing integer key\n");
    keyGeneric = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxIntFunc),
                                boxIntFunc, &keyValue, 1, "key_boxed");
  } else {
    fprintf(stderr, "[DICT DEBUG] dict_has: Key already boxed\n");
    keyGeneric = LLVMBuildIntToPtr(gen->builder, keyValue, genericPtrType, "key_ptr");
  }

  // Call franz_dict_has(dict, key_generic) -> i32
  LLVMValueRef dictHasFunc = LLVMGetNamedFunction(gen->module, "franz_dict_has");
  LLVMValueRef hasArgs[] = {dictPtr, keyGeneric};
  LLVMValueRef hasResult = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dictHasFunc), dictHasFunc, hasArgs, 2, "has_result");

  // Extend i32 to i64
  LLVMValueRef hasResultI64 = LLVMBuildZExt(gen->builder, hasResult, gen->intType, "has_result_i64");

  fprintf(stderr, "[DICT DEBUG] dict_has complete\n");
  return hasResultI64;
}

// ============================================================================
// (dict_keys dict) and (dict_values dict)
// ============================================================================

LLVMValueRef LLVMDict_compileDictKeys(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict_keys at line %d\n", lineNumber);

  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: dict_keys requires 1 argument (dict) at line %d\n", lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Compile and unbox dict
  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!dictValue) return NULL;

  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");
  LLVMValueRef unboxDictFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_dict");
  LLVMValueRef dictPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxDictFunc), unboxDictFunc, &dictGenericPtr, 1, "dict_ptr");

  // Allocate count variable
  LLVMValueRef countPtr = LLVMBuildAlloca(gen->builder, LLVMInt32TypeInContext(gen->context), "count_ptr");

  // Call franz_dict_keys(dict, &count) -> Generic**
  LLVMValueRef dictKeysFunc = LLVMGetNamedFunction(gen->module, "franz_dict_keys");
  LLVMValueRef keysArgs[] = {dictPtr, countPtr};
  LLVMValueRef keysArrayPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dictKeysFunc), dictKeysFunc, keysArgs, 2, "keys_array_ptr");

  // Load count
  LLVMValueRef count = LLVMBuildLoad2(gen->builder, LLVMInt32TypeInContext(gen->context), countPtr, "count");
  LLVMValueRef countI64 = LLVMBuildZExt(gen->builder, count, gen->intType, "count_i64");

  // Create list from keys array
  LLVMValueRef listNewFunc = LLVMGetNamedFunction(gen->module, "franz_list_new");
  LLVMValueRef listArgs[] = {keysArrayPtr, countI64};
  LLVMValueRef listPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNewFunc), listNewFunc, listArgs, 2, "list_ptr");

  // Convert to i64
  LLVMValueRef listAsInt = LLVMBuildPtrToInt(gen->builder, listPtr, gen->intType, "list_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_keys complete\n");
  return listAsInt;
}

LLVMValueRef LLVMDict_compileDictValues(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict_values at line %d\n", lineNumber);

  if (node->childCount != 2) {
    fprintf(stderr, "ERROR: dict_values requires 1 argument (dict) at line %d\n", lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Compile and unbox dict
  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!dictValue) return NULL;

  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");
  LLVMValueRef unboxDictFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_dict");
  LLVMValueRef dictPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxDictFunc), unboxDictFunc, &dictGenericPtr, 1, "dict_ptr");

  // Allocate count variable
  LLVMValueRef countPtr = LLVMBuildAlloca(gen->builder, LLVMInt32TypeInContext(gen->context), "count_ptr");

  // Call franz_dict_values(dict, &count) -> Generic**
  LLVMValueRef dictValuesFunc = LLVMGetNamedFunction(gen->module, "franz_dict_values");
  LLVMValueRef valuesArgs[] = {dictPtr, countPtr};
  LLVMValueRef valuesArrayPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dictValuesFunc), dictValuesFunc, valuesArgs, 2, "values_array_ptr");

  // Load count
  LLVMValueRef count = LLVMBuildLoad2(gen->builder, LLVMInt32TypeInContext(gen->context), countPtr, "count");
  LLVMValueRef countI64 = LLVMBuildZExt(gen->builder, count, gen->intType, "count_i64");

  // Create list from values array
  LLVMValueRef listNewFunc = LLVMGetNamedFunction(gen->module, "franz_list_new");
  LLVMValueRef listArgs[] = {valuesArrayPtr, countI64};
  LLVMValueRef listPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(listNewFunc), listNewFunc, listArgs, 2, "list_ptr");

  // Convert to i64
  LLVMValueRef listAsInt = LLVMBuildPtrToInt(gen->builder, listPtr, gen->intType, "list_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_values complete\n");
  return listAsInt;
}

// ============================================================================
// (dict_merge dict1 dict2)
// ============================================================================

LLVMValueRef LLVMDict_compileDictMerge(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);

  fprintf(stderr, "[DICT DEBUG] Compiling dict_merge at line %d\n", lineNumber);

  if (node->childCount != 3) {
    fprintf(stderr, "ERROR: dict_merge requires 2 arguments (dict1, dict2) at line %d\n", lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Compile and unbox dict1
  LLVMValueRef dict1Value = LLVMCodeGen_compileNode(gen, node->children[1]);
  if (!dict1Value) return NULL;

  LLVMValueRef dict1GenericPtr = LLVMBuildIntToPtr(gen->builder, dict1Value, genericPtrType, "dict1_generic_ptr");
  LLVMValueRef unboxDictFunc = LLVMGetNamedFunction(gen->module, "franz_unbox_dict");
  LLVMValueRef dict1Ptr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxDictFunc), unboxDictFunc, &dict1GenericPtr, 1, "dict1_ptr");

  // Compile and unbox dict2
  LLVMValueRef dict2Value = LLVMCodeGen_compileNode(gen, node->children[2]);
  if (!dict2Value) return NULL;

  LLVMValueRef dict2GenericPtr = LLVMBuildIntToPtr(gen->builder, dict2Value, genericPtrType, "dict2_generic_ptr");
  LLVMValueRef dict2Ptr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(unboxDictFunc), unboxDictFunc, &dict2GenericPtr, 1, "dict2_ptr");

  // Call franz_dict_merge(dict1, dict2) -> Generic*
  LLVMValueRef dictMergeFunc = LLVMGetNamedFunction(gen->module, "franz_dict_merge");
  LLVMValueRef mergeArgs[] = {dict1Ptr, dict2Ptr};
  LLVMValueRef mergedDictPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dictMergeFunc), dictMergeFunc, mergeArgs, 2, "merged_dict_ptr");

  // Convert to i64
  LLVMValueRef mergedDictAsInt = LLVMBuildPtrToInt(gen->builder, mergedDictPtr, gen->intType, "merged_dict_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_merge complete\n");
  return mergedDictAsInt;
}

// ============================================================================
// dict_map - Transform dictionary values
// ============================================================================
LLVMValueRef LLVMDict_compileDictMap(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  fprintf(stderr, "[DICT DEBUG] Compiling dict_map at line %d\n", lineNumber);
  fprintf(stderr, "[DICT DEBUG] Node childCount: %d\n", node->childCount);

  declareRuntimeDictFunctions(gen);
  fprintf(stderr, "[DICT DEBUG] After declareRuntimeDictFunctions\n");

  // childCount should be 3: children[0]=function name, children[1]=dict, children[2]=closure
  if (node->childCount < 3) {
    fprintf(stderr, "ERROR: dict_map requires 2 arguments (dict, function) at line %d\n", lineNumber);
    return NULL;
  }

  fprintf(stderr, "[DICT DEBUG] Creating genericPtrType\n");
  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  fprintf(stderr, "[DICT DEBUG] Looking for franz_dict_map function\n");
  // Declare franz_dict_map(dict_generic, closure_generic, lineNumber) -> Generic*
  LLVMValueRef dictMapFunc = LLVMGetNamedFunction(gen->module, "franz_dict_map");
  if (!dictMapFunc) {
    fprintf(stderr, "[DICT DEBUG] franz_dict_map not found, creating it\n");
    LLVMTypeRef paramTypes[] = {genericPtrType, genericPtrType, gen->intType};
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, paramTypes, 3, 0);
    dictMapFunc = LLVMAddFunction(gen->module, "franz_dict_map", funcType);
  }
  fprintf(stderr, "[DICT DEBUG] franz_dict_map function ready\n");

  // Compile dict argument (returns i64)
  // NOTE: children[0] is the function name, actual args start at children[1]
  AstNode *dictNode = node->children[1];
  fprintf(stderr, "[DICT DEBUG] Compiling dict argument (child 1: scores)\n");

  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, dictNode);
  if (!dictValue) {
    fprintf(stderr, "ERROR: Failed to compile dict argument for dict_map\n");
    return NULL;
  }

  // Convert dict i64 -> Generic* ptr
  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");

  // Compile closure argument (returns i64)
  AstNode *closureNode = node->children[2];
  LLVMValueRef closureValue = LLVMCodeGen_compileNode(gen, closureNode);
  if (!closureValue) {
    fprintf(stderr, "ERROR: Failed to compile closure argument for dict_map\n");
    return NULL;
  }

  // CRITICAL: The closure is a raw Closure* struct (as i64), NOT a Generic*
  // We need to box it into Generic* using franz_box_closure before passing to runtime

  // Declare franz_box_closure(ptr) -> ptr (Generic*)
  LLVMValueRef boxClosureFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
  if (!boxClosureFunc) {
    LLVMTypeRef boxType = LLVMFunctionType(genericPtrType, &genericPtrType, 1, 0);
    boxClosureFunc = LLVMAddFunction(gen->module, "franz_box_closure", boxType);
  }

  // Convert closure i64 -> Closure* ptr
  LLVMValueRef closurePtr = LLVMBuildIntToPtr(gen->builder, closureValue, genericPtrType, "closure_ptr");

  // Box the closure: franz_box_closure(closurePtr) -> Generic*
  LLVMValueRef closureGenericPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxClosureFunc),
                                                   boxClosureFunc, &closurePtr, 1, "closure_generic_ptr");

  // Line number as i64
  LLVMValueRef lineNum = LLVMConstInt(gen->intType, lineNumber, 0);

  // Call franz_dict_map(dict, closure, lineNumber)
  LLVMValueRef mapArgs[] = {dictGenericPtr, closureGenericPtr, lineNum};
  LLVMValueRef newDictPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dictMapFunc),
                                           dictMapFunc, mapArgs, 3, "new_dict_ptr");

  // Convert result to i64
  LLVMValueRef newDictAsInt = LLVMBuildPtrToInt(gen->builder, newDictPtr, gen->intType, "new_dict_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_map complete\n");
  return newDictAsInt;
}

// ============================================================================
// dict_filter - Filter dictionary entries
// ============================================================================
LLVMValueRef LLVMDict_compileDictFilter(LLVMCodeGen *gen, AstNode *node, int lineNumber) {
  declareRuntimeDictFunctions(gen);
  fprintf(stderr, "[DICT DEBUG] Compiling dict_filter at line %d\n", lineNumber);

  // childCount should be 3: children[0]=function name, children[1]=dict, children[2]=closure
  if (node->childCount < 3) {
    fprintf(stderr, "ERROR: dict_filter requires 2 arguments (dict, function) at line %d\n", lineNumber);
    return NULL;
  }

  LLVMTypeRef genericPtrType = LLVMPointerType(LLVMInt8TypeInContext(gen->context), 0);

  // Declare franz_dict_filter(dict_generic, closure_generic, lineNumber) -> Generic*
  LLVMValueRef dictFilterFunc = LLVMGetNamedFunction(gen->module, "franz_dict_filter");
  if (!dictFilterFunc) {
    LLVMTypeRef paramTypes[] = {genericPtrType, genericPtrType, gen->intType};
    LLVMTypeRef funcType = LLVMFunctionType(genericPtrType, paramTypes, 3, 0);
    dictFilterFunc = LLVMAddFunction(gen->module, "franz_dict_filter", funcType);
  }

  // Compile dict argument (returns i64)
  // NOTE: children[0] is the function name, actual args start at children[1]
  AstNode *dictNode = node->children[1];
  LLVMValueRef dictValue = LLVMCodeGen_compileNode(gen, dictNode);
  if (!dictValue) {
    fprintf(stderr, "ERROR: Failed to compile dict argument for dict_filter\n");
    return NULL;
  }

  // Convert dict i64 -> Generic* ptr
  LLVMValueRef dictGenericPtr = LLVMBuildIntToPtr(gen->builder, dictValue, genericPtrType, "dict_generic_ptr");

  // Compile closure argument (returns i64)
  AstNode *closureNode = node->children[2];
  LLVMValueRef closureValue = LLVMCodeGen_compileNode(gen, closureNode);
  if (!closureValue) {
    fprintf(stderr, "ERROR: Failed to compile closure argument for dict_filter\n");
    return NULL;
  }

  // CRITICAL: The closure is a raw Closure* struct (as i64), NOT a Generic*
  // We need to box it into Generic* using franz_box_closure before passing to runtime

  // Declare franz_box_closure(ptr) -> ptr (Generic*)
  LLVMValueRef boxClosureFunc = LLVMGetNamedFunction(gen->module, "franz_box_closure");
  if (!boxClosureFunc) {
    LLVMTypeRef boxType = LLVMFunctionType(genericPtrType, &genericPtrType, 1, 0);
    boxClosureFunc = LLVMAddFunction(gen->module, "franz_box_closure", boxType);
  }

  // Convert closure i64 -> Closure* ptr
  LLVMValueRef closurePtr = LLVMBuildIntToPtr(gen->builder, closureValue, genericPtrType, "closure_ptr");

  // Box the closure: franz_box_closure(closurePtr) -> Generic*
  LLVMValueRef closureGenericPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(boxClosureFunc),
                                                   boxClosureFunc, &closurePtr, 1, "closure_generic_ptr");

  // Line number as i64
  LLVMValueRef lineNum = LLVMConstInt(gen->intType, lineNumber, 0);

  // Call franz_dict_filter(dict, closure, lineNumber)
  LLVMValueRef filterArgs[] = {dictGenericPtr, closureGenericPtr, lineNum};
  LLVMValueRef filteredDictPtr = LLVMBuildCall2(gen->builder, LLVMGlobalGetValueType(dictFilterFunc),
                                                dictFilterFunc, filterArgs, 3, "filtered_dict_ptr");

  // Convert result to i64
  LLVMValueRef filteredDictAsInt = LLVMBuildPtrToInt(gen->builder, filteredDictPtr, gen->intType, "filtered_dict_as_i64");

  fprintf(stderr, "[DICT DEBUG] dict_filter complete\n");
  return filteredDictAsInt;
}
