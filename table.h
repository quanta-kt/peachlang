#ifndef peach_table_h
#define peach_table_h

#include "common.h"
#include "value.h"

typedef struct {
  ObjectString* key;
  Value value;
} Entry;

typedef struct {
  size_t count;
  size_t capacity;
  Entry* entries;
} Table;


void Table_init(Table* table);
void Table_free(Table* table);

/**
 * Set/update an entry in the table.
 * Returns true if a new entry was created, or false Otherwise.
 */
bool Table_set(Table* table, ObjectString* key, Value value);

/**
 * Retrive a value from table belogning to given key.
 *
 * If the key exists in the table, `value` is updated to point
 * to the value it is set to and true is returned.
 * Otherwise, false is returned and `value` is left unchanged.
 */
bool Table_get(Table* table, ObjectString* key, Value* value);

/**
 * Delete an entry from the table.
 * Returns false if the given key did not exist in the table
 * and nothing was deleted. True otherwise.
 */
bool Table_delete(Table *table, ObjectString *key);

/**
 * Adds all entries from the `source` table to a
 * table.
 */
void Table_add_all(Table* table, Table* source);

void Table_print(Table* table);


ObjectString* Table_find_str(Table* table, const char* str, size_t length);

#endif // !peach_table_h

