#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dict.h"
#include "generic.h"
#include "list.h"

#define DICT_INITIAL_CAPACITY 16
#define DICT_LOAD_FACTOR 0.75

// Hash function for Generic keys
// Uses FNV-1a hash algorithm
unsigned int Dict_hash(Generic *key, int capacity) {
  unsigned int hash = 2166136261u;

  if (key->type == TYPE_STRING) {
    char *str = *((char **) key->p_val);
    for (int i = 0; str[i] != '\0'; i++) {
      hash ^= (unsigned char)str[i];
      hash *= 16777619u;
    }
  } else if (key->type == TYPE_INT) {
    int val = *((int *) key->p_val);
    hash ^= val;
    hash *= 16777619u;
  } else if (key->type == TYPE_FLOAT) {
    double val = *((double *) key->p_val);
    // Hash the bytes of the double
    unsigned char *bytes = (unsigned char *)&val;
    for (size_t i = 0; i < sizeof(double); i++) {
      hash ^= bytes[i];
      hash *= 16777619u;
    }
  }
  // For other types, use pointer address as hash
  else {
    unsigned long addr = (unsigned long)key->p_val;
    hash ^= (unsigned int)addr;
    hash *= 16777619u;
  }

  return hash % capacity;
}

// Create new dictionary with specified capacity
Dict *Dict_new(int initial_capacity) {
  if (initial_capacity <= 0) {
    initial_capacity = DICT_INITIAL_CAPACITY;
  }

  Dict *dict = (Dict *) malloc(sizeof(Dict));
  dict->capacity = initial_capacity;
  dict->size = 0;
  dict->buckets = (DictEntry **) calloc(initial_capacity, sizeof(DictEntry *));

  return dict;
}

// Free dictionary and all entries
void Dict_free(Dict *dict) {
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      DictEntry *next = entry->next;

      // Decrement refCounts (Dict was owning them)
      entry->key->refCount--;
      entry->value->refCount--;

      // Free key and value if no more references
      if (entry->key->refCount == 0) Generic_free(entry->key);
      if (entry->value->refCount == 0) Generic_free(entry->value);

      free(entry);
      entry = next;
    }
  }

  free(dict->buckets);
  free(dict);
}

// Deep copy dictionary
Dict *Dict_copy(Dict *dict) {
  Dict *new_dict = Dict_new(dict->capacity);

  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      // Copy key and value and insert into new dict using in-place
      Dict_set_inplace(new_dict, entry->key, entry->value);
      entry = entry->next;
    }
  }

  return new_dict;
}

// Print dictionary in {key: value, ...} format
void Dict_print(Dict *dict) {
  printf("{");

  int printed = 0;
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      if (printed > 0) printf(", ");

      Generic_print(entry->key);
      printf(": ");
      Generic_print(entry->value);

      printed++;
      entry = entry->next;
    }
  }

  printf("}");
  fflush(stdout);
}

// Get value for key (returns NULL if not found)
Generic *Dict_get(Dict *dict, Generic *key) {
  unsigned int index = Dict_hash(key, dict->capacity);
  DictEntry *entry = dict->buckets[index];

  while (entry != NULL) {
    if (Generic_is(entry->key, key)) {
      return entry->value;
    }
    entry = entry->next;
  }

  return NULL;
}

// Check if key exists
int Dict_has(Dict *dict, Generic *key) {
  return Dict_get(dict, key) != NULL ? 1 : 0;
}

// Resize dictionary when load factor exceeded (mutates in place)
static void Dict_resize_inplace(Dict *dict) {
  int new_capacity = dict->capacity * 2;
  DictEntry **new_buckets = (DictEntry **) calloc(new_capacity, sizeof(DictEntry *));

  // Rehash all entries
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      DictEntry *next = entry->next;

      // Rehash
      unsigned int new_index = Dict_hash(entry->key, new_capacity);

      // Insert at head of new bucket
      entry->next = new_buckets[new_index];
      new_buckets[new_index] = entry;

      entry = next;
    }
  }

  free(dict->buckets);
  dict->buckets = new_buckets;
  dict->capacity = new_capacity;
}

// Resize dictionary when load factor exceeded (returns new dict)
static Dict *Dict_resize(Dict *dict) {
  int new_capacity = dict->capacity * 2;
  DictEntry **new_buckets = (DictEntry **) calloc(new_capacity, sizeof(DictEntry *));

  // Rehash all entries
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      DictEntry *next = entry->next;

      // Rehash
      unsigned int new_index = Dict_hash(entry->key, new_capacity);

      // Insert at head of new bucket
      entry->next = new_buckets[new_index];
      new_buckets[new_index] = entry;

      entry = next;
    }
  }

  free(dict->buckets);
  dict->buckets = new_buckets;
  dict->capacity = new_capacity;

  return dict;
}

// Set key-value pair in place (mutates dict, for efficient building)
void Dict_set_inplace(Dict *dict, Generic *key, Generic *value) {
  // Check if we need to resize
  if ((double)dict->size / dict->capacity > DICT_LOAD_FACTOR) {
    Dict_resize_inplace(dict);
  }

  unsigned int index = Dict_hash(key, dict->capacity);
  DictEntry *entry = dict->buckets[index];

  // Check if key already exists
  while (entry != NULL) {
    if (Generic_is(entry->key, key)) {
      // Update existing entry
      // Decrement old value's refCount first
      entry->value->refCount--;
      if (entry->value->refCount == 0) Generic_free(entry->value);

      // Set new value and increment refCount (Dict owns the value)
      entry->value = Generic_copy(value);
      entry->value->refCount++;
      return;
    }
    entry = entry->next;
  }

  // Key doesn't exist, create new entry
  DictEntry *new_entry = (DictEntry *) malloc(sizeof(DictEntry));
  new_entry->key = Generic_copy(key);
  new_entry->value = Generic_copy(value);

  // Dict owns both key and value (increment refCounts)
  new_entry->key->refCount++;
  new_entry->value->refCount++;

  new_entry->next = dict->buckets[index];
  dict->buckets[index] = new_entry;
  dict->size++;
}

// Set key-value pair (returns new dict, original unchanged for immutability)
Dict *Dict_set(Dict *dict, Generic *key, Generic *value) {
  // Create copy for immutability
  Dict *new_dict = Dict_copy(dict);

  // Check if we need to resize
  if ((double)new_dict->size / new_dict->capacity > DICT_LOAD_FACTOR) {
    new_dict = Dict_resize(new_dict);
  }

  unsigned int index = Dict_hash(key, new_dict->capacity);
  DictEntry *entry = new_dict->buckets[index];

  // Check if key already exists
  while (entry != NULL) {
    if (Generic_is(entry->key, key)) {
      // Update existing entry
      if (entry->value->refCount == 0) Generic_free(entry->value);
      entry->value = Generic_copy(value);
      return new_dict;
    }
    entry = entry->next;
  }

  // Key doesn't exist, create new entry
  DictEntry *new_entry = (DictEntry *) malloc(sizeof(DictEntry));
  new_entry->key = Generic_copy(key);
  new_entry->value = Generic_copy(value);
  new_entry->next = new_dict->buckets[index];
  new_dict->buckets[index] = new_entry;
  new_dict->size++;

  return new_dict;
}

// Remove key (returns new dict, original unchanged)
Dict *Dict_remove(Dict *dict, Generic *key) {
  // Create copy for immutability
  Dict *new_dict = Dict_copy(dict);

  unsigned int index = Dict_hash(key, new_dict->capacity);
  DictEntry *entry = new_dict->buckets[index];
  DictEntry *prev = NULL;

  while (entry != NULL) {
    if (Generic_is(entry->key, key)) {
      // Remove entry
      if (prev == NULL) {
        new_dict->buckets[index] = entry->next;
      } else {
        prev->next = entry->next;
      }

      // Free entry
      if (entry->key->refCount == 0) Generic_free(entry->key);
      if (entry->value->refCount == 0) Generic_free(entry->value);
      free(entry);

      new_dict->size--;
      return new_dict;
    }
    prev = entry;
    entry = entry->next;
  }

  // Key not found, return unchanged dict
  return new_dict;
}

// Get size of dictionary
int Dict_size(Dict *dict) {
  return dict->size;
}

// Get all keys as array (caller must free array but not contents)
Generic **Dict_keys(Dict *dict, int *count) {
  *count = dict->size;
  Generic **keys = (Generic **) malloc(sizeof(Generic *) * dict->size);

  int idx = 0;
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      keys[idx++] = entry->key;
      entry = entry->next;
    }
  }

  return keys;
}

// Get all values as array (caller must free array but not contents)
Generic **Dict_values(Dict *dict, int *count) {
  *count = dict->size;
  Generic **values = (Generic **) malloc(sizeof(Generic *) * dict->size);

  int idx = 0;
  for (int i = 0; i < dict->capacity; i++) {
    DictEntry *entry = dict->buckets[i];
    while (entry != NULL) {
      values[idx++] = entry->value;
      entry = entry->next;
    }
  }

  return values;
}

// Merge two dictionaries (dict2 values overwrite dict1 on conflict)
Dict *Dict_merge(Dict *dict1, Dict *dict2) {
  // Start with copy of dict1
  Dict *result = Dict_copy(dict1);

  // Add all entries from dict2 using in-place mutation
  for (int i = 0; i < dict2->capacity; i++) {
    DictEntry *entry = dict2->buckets[i];
    while (entry != NULL) {
      Dict_set_inplace(result, entry->key, entry->value);
      entry = entry->next;
    }
  }

  return result;
}

// Compare two dicts for equality
int Dict_compare(Dict *dict1, Dict *dict2) {
  if (dict1->size != dict2->size) return 0;

  // Check all entries in dict1 exist in dict2 with same values
  for (int i = 0; i < dict1->capacity; i++) {
    DictEntry *entry = dict1->buckets[i];
    while (entry != NULL) {
      Generic *value2 = Dict_get(dict2, entry->key);
      if (value2 == NULL || !Generic_is(entry->value, value2)) {
        return 0;
      }
      entry = entry->next;
    }
  }

  return 1;
}
