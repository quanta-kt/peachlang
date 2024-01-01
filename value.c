#include "memory.h"
#include "value.h"
#include "object.h"

#include <stdio.h>
#include <string.h>

void Value_print(Value value) {
  switch (value.type) {
    case VAL_NIL: printf("nil"); break;
    case VAL_BOOL: printf(AS_BOOL(value) ? "true" : "false"); break;
    case VAL_NUMBER: printf("%g", AS_NUMBER(value)); break;
    case VAL_OBJECT: Object_print(value); break;
    default: printf("unkown value type");
  }
}

bool Value_equals(Value value, Value other) {
  if (value.type != other.type) return false;

  switch (value.type) {
    case VAL_NIL:    return true;
    case VAL_BOOL:   return AS_BOOL(value) == AS_BOOL(other);
    case VAL_NUMBER: return AS_NUMBER(value) == AS_NUMBER(other);
    case VAL_OBJECT: return AS_STRING(value) == AS_STRING(other);
    default:         return false;
  }
}

void ValueArray_init(ValueArray* array) {
  array->values = NULL;
  array->capacity = 0;
  array->count = 0;
}

void ValueArray_write(ValueArray *array, Value value) {
  if (array->capacity < array->count + 1) {
    size_t old_capacity = array->capacity;
    array->capacity = GROW_CAPACITY(old_capacity);
    array->values = GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
  }

  array->values[array->count] = value;
  array->count++;
}

void ValueArray_free(ValueArray* array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  ValueArray_init(array);
}

