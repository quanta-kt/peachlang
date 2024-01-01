#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"


#define ALLOCATE_OBJECT(type, object_type, vm) \
  (type*) Object_create(vm, sizeof(type), object_type)


static Object* Object_create(size_t size, ObjectType type) {
  Object* object = (Object*) reallocate(NULL, 0, size);
  object->type = type;
  return object;
}

uint32_t string_hash(const char* str, size_t length) {
  uint32_t hash = 2166136261u;

  for (size_t i = 0; i < length; i++) {
    hash ^= (uint32_t) str[i];
    hash *= 16777619;
  }

  return hash;
}

ObjectString* ObjectString_take(char* chars, size_t length) {
  ObjectString* string = (ObjectString*) Object_create(sizeof(ObjectString), OBJ_STRING);
  string->length = length;
  string->chars = chars;
  string->hash = string_hash(chars, length);

  return string;
}

ObjectString* ObjectString_copy(const char* chars, size_t length) {
  uint32_t hash = string_hash(chars, length);
  char* buffer = ALLOCATE(char, length + 1);

  memcpy(buffer, chars, length);
  buffer[length] = '\0';

  return ObjectString_take(buffer, length);
}

void Object_print(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
  }
}

