#ifndef DICT_H
#define DICT_H
#include "generic.h"

// Hash table entry (key-value pair with collision chaining)
typedef struct DictEntry {
  Generic *key;
  Generic *value;
  struct DictEntry *next;  // For collision resolution via chaining
} DictEntry;

// Hash table dictionary
typedef struct Dict {
  DictEntry **buckets;  // Array of bucket chains
  int capacity;         // Number of buckets
  int size;            // Number of key-value pairs
} Dict;

// Prototypes
Dict *Dict_new(int initial_capacity);
void Dict_free(Dict *dict);
Dict *Dict_copy(Dict *dict);
void Dict_print(Dict *dict);

unsigned int Dict_hash(Generic *key, int capacity);
Generic *Dict_get(Dict *dict, Generic *key);
void Dict_set_inplace(Dict *dict, Generic *key, Generic *value);
Dict *Dict_set(Dict *dict, Generic *key, Generic *value);
Dict *Dict_remove(Dict *dict, Generic *key);
int Dict_has(Dict *dict, Generic *key);
int Dict_size(Dict *dict);

// Helper to get all keys/values as lists
Generic **Dict_keys(Dict *dict, int *count);
Generic **Dict_values(Dict *dict, int *count);

// Merge two dicts
Dict *Dict_merge(Dict *dict1, Dict *dict2);

// Compare two dicts for equality
int Dict_compare(Dict *dict1, Dict *dict2);

#endif
