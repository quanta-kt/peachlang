#ifndef peach_object_h
#define peach_object_h

#include "common.h"
#include "value.h"
#include "vm.h"

typedef enum {
  OBJ_STRING,
} ObjectType;

struct Object {
  ObjectType type;
  struct Object* next;
};

struct ObjectString {
  Object object;
  size_t length;
  uint32_t hash;
  char* chars;
};

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value)   is_object_type(value, OBJ_STRING)

#define AS_STRING(value)   ((ObjectString*) AS_OBJECT(value))
#define AS_CSTRING(value)  (((ObjectString*) AS_OBJECT(value))->chars)

static inline bool is_object_type(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

void Object_print(Value value);


/**
 * Allocates an ObjectString object.
 *
 * This function does NOT initialize the string, add it to interned string table
 * or the hash -- the caller is responsible for doing so.
 */
ObjectString* ObjectString_create();

/**
 * Allocates an ObjectString Object and takes the ownership of the given C-string.
 */
ObjectString* ObjectString_take(char* chars, size_t length);

/**
 * Allocates an ObjectString object and initializes it with the given C-string.
 */
ObjectString* ObjectString_copy(const char* chars, size_t length);

uint32_t string_hash(const char* str, size_t length);

#endif // !peach_object_h

