#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "object.h"
#include "memory.h"
#include "value.h"
#include "vm.h"


#define ALLOCATE_OBJECT(type, object_type) \
  (type*) Object_create(sizeof(type), object_type)


static Object* Object_create(size_t size, ObjectType type) {
  Object* object = (Object*) reallocate(NULL, 0, size);
  object->type = type;
  return object;
}

ObjectString* ObjectString_create(char* chars, size_t length) {
  ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJ_STRING);
  string->length = length;
  string->chars = chars;
  return string;
}

ObjectString* ObjectString_from_cstring(const char* chars, size_t length) {
  char* heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';
  return ObjectString_create(heap_chars, length);
}

void Object_print(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
  }
}