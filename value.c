#include "memory.h"
#include "value.h"

#include <stdio.h>

void Value_print(Value value) {
  printf("%g", value);
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

