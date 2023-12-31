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
  char chars[];
};

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value)   is_object_type(value, OBJ_STRING)

#define AS_STRING(value)   ((ObjectString*) AS_OBJECT(value))
#define AS_CSTRING(value)  (((ObjectString*) AS_OBJECT(value))->chars)

static inline bool is_object_type(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

void Object_print(Value value);

ObjectString* ObjectString_from_cstring(VM* vm, const char* chars, size_t length);
ObjectString* ObjectString_create(VM* vm, size_t length);

#endif // !peach_object_h

