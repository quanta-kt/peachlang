#ifndef peach_value_h
#define peach_value_h

#include "common.h"

typedef double Value;

void Value_print(Value value);

typedef struct {
  size_t capacity;
  size_t count;
  Value* values;
} ValueArray;

void ValueArray_init(ValueArray* array);

void ValueArray_write(ValueArray* array, Value value);

void ValueArray_free(ValueArray* array);

#endif // !peach_value_h

