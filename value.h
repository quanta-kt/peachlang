#ifndef peach_value_h
#define peach_value_h

#include "common.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#define BOOL_VAL(value)   ((Value) {VAL_BOOL, {.boolean = value}})
#define NIL_VAL           ((Value) {VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value) {VAL_NUMBER, {.number = value}})

#define AS_BOOL(value)    ((value).as.boolean)
#define AS_NUMBER(value)  ((value).as.number)

#define IS_BOOL(value)    ((value).type == VAL_BOOL)
#define IS_NIL(value)     ((value).type == VAL_NIL)
#define IS_NUMBER(value)  ((value).type == VAL_NUMBER)

void Value_print(Value value);
bool Value_equals(Value value, Value other);


typedef struct {
  size_t capacity;
  size_t count;
  Value* values;
} ValueArray;


void ValueArray_init(ValueArray* array);

void ValueArray_write(ValueArray* array, Value value);

void ValueArray_free(ValueArray* array);

#endif // !peach_value_h

