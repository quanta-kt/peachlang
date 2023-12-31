#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h" 
#include "debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static void concatenate(VM* vm);
static bool is_falsey(Value value);
static void runtime_error(VM* vm, const char* fmt, ...);
static void push(VM* vm, Value value);
static Value pop(VM* vm);
static Value peek(VM* vm, size_t depth);
static void reset_stack(VM* vm);

void VM_init(VM* vm) {
  reset_stack(vm);
}

static InterpretResult run(VM* vm) {
  #define READ_BYTE() (*(vm->ip++))

  #define READ_CONSTANT() (vm->chunk->constants.values[READ_BYTE()])

  #define READ_CONSTANT_LONG() ( \
    vm->chunk->constants.values[ \
      READ_BYTE() \
      | (READ_BYTE() << 8) \
      | (READ_BYTE() << 16) \
    ])
  
  #define BINARY_OP(type_value, op) \
    do { \
      \
      if (!IS_NUMBER(peek(vm, 0)) || !IS_NUMBER(peek(vm, 1))) { \
        runtime_error(vm, "Operands must be numbers."); \
      } \
      \
      double b = AS_NUMBER(pop(vm)); \
      double a = AS_NUMBER(pop(vm)); \
      push(vm, type_value(a op b)); \
    } while(false)

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
      printf("[ ");
      Value_print(*slot);
      printf(" ]");
    }
    printf("\n");

    disassemble_instruction(vm->chunk, (size_t)(vm->ip - vm->chunk->code));
    #endif /* ifdef DEBUG_TRACE_EXECUTION */

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
      case OP_RETURN: {
        Value_print(pop(vm));
        printf("\n");
        return INTERPRET_OK;
      }

      case OP_LOAD_CONST: {
        Value constant = READ_CONSTANT();
        push(vm, constant);
        break;
      }

      case OP_LOAD_CONST_LONG: {
        Value constant = READ_CONSTANT_LONG();
        push(vm, constant);
        break;
      }

      case OP_NIL:   push(vm, NIL_VAL); break;
      case OP_FALSE: push(vm, BOOL_VAL(false)); break;
      case OP_TRUE:  push(vm, BOOL_VAL(true)); break;

      case OP_EQUAL: {
        Value a = pop(vm);
        Value b = pop(vm);
        push(vm, BOOL_VAL(Value_equals(a, b)));
        break;
      }
      case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:    BINARY_OP(BOOL_VAL, <); break;

      case OP_NEGATE: { 
        if (!IS_NUMBER(peek(vm, 0))) {
          runtime_error(vm, "Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(vm, NUMBER_VAL(-AS_NUMBER(pop(vm))));
        break;
      }

      case OP_ADD: {
        if (IS_STRING(peek(vm, 0)) && IS_STRING(peek(vm, 1))) {
          concatenate(vm);
        } else if (IS_NUMBER(peek(vm, 0)) && IS_NUMBER(peek(vm, 1))) {
          BINARY_OP(NUMBER_VAL, +);
        } else {
          runtime_error(vm, "Operands must be two numbers or two strings.");
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }
      case OP_SUB: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MUL: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIV: BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT: push(vm, BOOL_VAL(is_falsey(pop(vm)))); break;
    }
  }

  #undef READ_BYTE
  #undef READ_CONSTANT
  #undef READ_CONSTANT_LONG
}

InterpretResult VM_interpret(VM* vm, const char *source) {
  Chunk chunk;
  Chunk_init(&chunk);

  if (!compile(source, &chunk)) {
    Chunk_free(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm->chunk = &chunk;
  vm->ip = vm->chunk->code;

  InterpretResult result = run(vm);

  Chunk_free(&chunk);
  return result;
}

void VM_free(VM* vm) {}

static void concatenate(VM* vm) {
  ObjectString* b = AS_STRING(pop(vm));
  ObjectString* a = AS_STRING(pop(vm));
  size_t length = a->length + b->length;

  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjectString* result = ObjectString_create(chars, length);
  push(vm, OBJECT_VAL(result));
}

static bool is_falsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void runtime_error(VM* vm, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  size_t instruction = vm->ip - vm->chunk->code - 1;
  size_t line = Chunk_get_line(vm->chunk, instruction);
  fprintf(stderr, "[line %zu] in script\n", line);

  reset_stack(vm);
}

static void push(VM* vm, Value value) {
  *vm->stack_top = value;
  vm->stack_top++;
}

static Value pop(VM* vm) {
  vm->stack_top--;
  return *vm->stack_top;
}

static Value peek(VM* vm, size_t depth) {
  return vm->stack_top[-1 - depth];
}

static void reset_stack(VM* vm) {
  vm->stack_top = vm->stack;
}

