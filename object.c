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

  #ifdef DEBUG_LOG_GC
    printf("%p allocate %zu for %d\n", (void*) object, size, type);
  #endif

  return object;
}

uint32_t string_hash(uint32_t start, const char* str, size_t length) {
  uint32_t hash = start;

  for (size_t i = 0; i < length; i++) {
    hash ^= (uint32_t) str[i];
    hash *= 16777619;
  }

  return hash;
}

/**
 * Allocates an ObjectString object.
 *
 * This function does NOT initialize the string, add it to interned string table
 * or the hash -- the caller is responsible for doing so.
 */
static ObjectString* ObjectString_create() {
  ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJ_STRING); 
  string->chars = "";
  string->length = 0;
  string->hash = 0;

  return string;
}

ObjectString* ObjectString_take(char* chars, size_t length) {
  ObjectString* string = ObjectString_create();
  string->length = length;
  string->chars = chars;
  string->hash = string_hash(STRING_HASH_INIT, chars, length);

  return string;
}

ObjectString* ObjectString_copy(const char* chars, size_t length) {
  uint32_t hash = string_hash(STRING_HASH_INIT, chars, length);
  char* buffer = ALLOCATE(char, length + 1);

  memcpy(buffer, chars, length);
  buffer[length] = '\0';

  return ObjectString_take(buffer, length);
}

ObjectUpvalue* ObjectUpvalue_create(Value* slot) {
  ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJ_UPVALUE);
  upvalue->location = slot;
  upvalue->closed = NIL_VAL;
  upvalue->next = NULL;
  return upvalue;
}

ObjectFunction* ObjectFunction_create() {
  ObjectFunction* fn = ALLOCATE_OBJECT(ObjectFunction, OBJ_FUNCTION);
  fn->arity = 0;
  fn->name = NULL;
  Chunk_init(&fn->chunk);
  return fn;
}

ObjectClosure* ObjectClosure_crate(ObjectFunction* function) {
  ObjectUpvalue** upvalues = ALLOCATE(ObjectUpvalue*, function->upvalue_count);
  for (size_t i = 0; i < function->upvalue_count; i++) {
    upvalues[i] = NULL;
  }

  ObjectClosure* closure = ALLOCATE_OBJECT(ObjectClosure, OBJ_CLOSURE);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalue_count = function->upvalue_count;
  return closure;
}

ObjectFunction* ObjectNativeFn_create(NativeFn function) {
  ObjectNativeFn* native_fn = ALLOCATE_OBJECT(ObjectNativeFn, OBJ_NATIVE_FN);
  native_fn->function = function;
}

void print_function(ObjectFunction* fn) {
  if (fn->name == NULL) {
    printf("<script>");
    return;
  }

  printf("<fn %s>", fn->name->chars);
}

void Object_print(Value value) {
  switch (OBJECT_TYPE(value)) {
    case OBJ_STRING: printf("%s", AS_CSTRING(value)); break;
    case OBJ_UPVALUE: printf("upvalue"); break;
    case OBJ_FUNCTION: print_function(AS_FUNCTION(value)); break;
    case OBJ_CLOSURE: print_function(AS_CLOSURE(value)->function); break;
    case OBJ_NATIVE_FN: printf("<native fn>"); break;
  }
}

