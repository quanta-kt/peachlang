#ifndef peach_object_h
#define peach_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define STRING_HASH_INIT 2166136261

typedef enum {
  OBJ_STRING,
  OBJ_UPVALUE,
  OBJ_FUNCTION,
  OBJ_CLOSURE,
  OBJ_NATIVE_FN,
} ObjectType;

struct Object {
  ObjectType type;
  struct Object* next;
};

typedef struct ObjectUpvalue {
  Object object;
  Value* location;
  Value closed;
  struct ObjectUpvalue* next;
} ObjectUpvalue;


typedef struct {
  Object object;
  size_t arity;
  Chunk chunk;
  ObjectString* name;
  uint8_t upvalue_count;
} ObjectFunction;

typedef struct {
  Object object;
  ObjectFunction* function;
  ObjectUpvalue** upvalues;
  uint8_t upvalue_count;
} ObjectClosure;

typedef Value (*NativeFn) (size_t arg_count, Value* args);

typedef struct {
  Object object;
  NativeFn function;
} ObjectNativeFn;

struct ObjectString {
  Object object;
  size_t length;
  uint32_t hash;
  char* chars;
};

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_FUNCTION(value) is_object_type(value, OBJ_FUNCTION)
#define IS_CLOSURE(value) is_object_type(value, OBJ_CLOSURE)
#define IS_NATIVE_FN(value) is_object_type(value, OBJ_NATIVE_FN)
#define IS_STRING(value)   is_object_type(value, OBJ_STRING)

#define AS_FUNCTION(value) ((ObjectFunction*) AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjectClosure*) AS_OBJECT(value))
#define AS_NATIVE_FN(value) (((ObjectNativeFn*) AS_OBJECT(value))->function)
#define AS_STRING(value)   ((ObjectString*) AS_OBJECT(value))
#define AS_CSTRING(value)  (((ObjectString*) AS_OBJECT(value))->chars)

static inline bool is_object_type(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

void Object_print(Value value);

/**
 * Allocates an ObjectString Object and takes the ownership of the given C-string.
 */
ObjectString* ObjectString_take(char* chars, size_t length);

/**
 * Allocates an ObjectString object and initializes it with the given C-string.
 */
ObjectString* ObjectString_copy(const char* chars, size_t length);

ObjectUpvalue* ObjectUpvalue_create(Value* slot);

ObjectFunction* ObjectFunction_create();

ObjectClosure* ObjectClosure_crate(ObjectFunction* function);

ObjectFunction* ObjectNativeFn_create(NativeFn fn);

uint32_t string_hash(uint32_t start, const char* str, size_t length);

#endif // !peach_object_h

