#include "table.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define TABLE_MAX_LOAD 0.75


static Entry* find_entry(Entry* entries, size_t capacity, ObjectString* key);
static void adjust_capacity(Table* table, size_t capacity);

void Table_init(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void Table_free(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  Table_init(table);
}

bool Table_set(Table* table, ObjectString* key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    size_t capacity = GROW_CAPACITY(table->capacity);
    adjust_capacity(table, capacity);
  }

  Entry* entry = find_entry(table->entries, table->capacity, key);
  bool is_new_key = entry->key == NULL;
  if (is_new_key && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return is_new_key;
}

bool Table_get(Table* table, ObjectString* key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  *value = entry->value;
  return true;
}

bool Table_delete(Table *table, ObjectString *key) {
  if (table->count == 0) return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL) return false;

  entry->key = NULL;
  entry->value = BOOL_VAL(false);
  return true;
}

void Table_add_all(Table* table, Table* source) {
  for (size_t i = 0; i < source->capacity; i++) {
    Entry* entry = &source->entries[i];
    if (entry->key == NULL) continue;

    Table_set(table, entry->key, entry->value);
  }
}

static Entry* find_entry(Entry* entries, size_t capacity, ObjectString* key) {
  uint32_t index = key->hash % capacity;
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) {
        return tombstone != NULL ? tombstone : entry;
      } else {
        // we found a tombstone
        tombstone = entry;
      }
    } else if (entry->key == key) {
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

ObjectString* Table_find_str(Table* table, const char* str, size_t length) {
  if (table->count == 0) return NULL;

  uint32_t hash = string_hash(STRING_HASH_INIT, str, length);
  uint32_t index = hash % table->capacity;

  for (;;) {
    Entry* entry = &table->entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return NULL;
    } else {
      if (
        entry->key->length == length &&
        entry->key->hash == hash &&
        memcmp(entry->key->chars, str, length) == 0
      ) {
        return entry->key;
      }
    }

    index = (index + 1) % table->capacity;
  }
}

ObjectString* Table_find_str_combined(
  Table* table, const char* a, size_t a_len, 
  const char* b, size_t b_len
) { 
  if (table->count == 0) return NULL;

  uint32_t hash = string_hash(STRING_HASH_INIT, a, a_len);
  hash = string_hash(hash, b, b_len);
  uint32_t index = hash % table->capacity;
  size_t length = a_len + b_len; 

  for (;;) {
    Entry* entry = &table->entries[index];

    if (entry->key == NULL) {
      if (IS_NIL(entry->value)) return NULL;
    } else {
      if (
        entry->key->length == length &&
        entry->key->hash == hash &&
        memcmp(entry->key->chars, a, a_len) == 0 &&
        memcmp(entry->key->chars + a_len, b, b_len) == 0
      ) {
        return entry->key;
      }
    }

    index = (index + 1) % table->capacity;
  }
}

void Table_print(Table* table) {
  printf("{");
  if (table->count == 0) {
    goto end;
  }

  bool is_first = true;

  for (size_t i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue;

    if (!is_first) {
      printf(", ");
    }

    printf("\"%s\": ", entry->key->chars);
    Value_print(entry->value);

    is_first = false;
  }

  end:
  printf("}");
}

static void adjust_capacity(Table* table, size_t capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);

  for (size_t i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }

  // copy over original entires
  table->count = 0;
  for (size_t i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key == NULL) continue; 

    Entry* dest = find_entry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;

    table->count++;
  }

  // free original array
  FREE_ARRAY(Entry, table->entries, table->capacity);

  table->entries = entries;
  table->capacity = capacity;
}

