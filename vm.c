#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "value.h"
#include "vm.h" 
#include "debug.h"

#include <stdio.h>
#include <stdarg.h>

VM vm;

static bool is_falsey(Value value);
static void runtime_error(VM* vm, const char* fmt, ...);
static void push(Value value);
static Value pop();
static Value peek(size_t depth);
static void reset_stack();

void VM_init() {
  reset_stack();
}

static InterpretResult run() {
  #define READ_BYTE() (*vm.ip++)

  #define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

  #define READ_CONSTANT_LONG() ( \
    vm.chunk->constants.values[ \
      READ_BYTE() \
      | (READ_BYTE() << 8) \
      | (READ_BYTE() << 16) \
    ])
  
  #define BINARY_OP(type_value, op) \
    do { \
      \
      if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
        runtime_error(&vm, "Operands must be numbers."); \
      } \
      \
      double b = AS_NUMBER(pop()); \
      double a = AS_NUMBER(pop()); \
      push(type_value(a op b)); \
    } while(false)

  for (;;) {
    #ifdef DEBUG_TRACE_EXECUTION
    printf("          ");
    for (Value* slot = vm.stack; slot < vm.stack_top; slot++) {
      printf("[ ");
      Value_print(*slot);
      printf(" ]");
    }
    printf("\n");

    disassemble_instruction(vm.chunk, (size_t)(vm.ip - vm.chunk->code));
    #endif /* ifdef DEBUG_TRACE_EXECUTION */

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
      case OP_RETURN: {
        Value_print(pop());
        printf("\n");
        return INTERPRET_OK;
      }

      case OP_LOAD_CONST: {
        Value constant = READ_CONSTANT();
        push(constant);
        break;
      }

      case OP_LOAD_CONST_LONG: {
        Value constant = READ_CONSTANT_LONG();
        push(constant);
        break;
      }

      case OP_NIL:   push(NIL_VAL); break;
      case OP_FALSE: push(BOOL_VAL(false)); break;
      case OP_TRUE:  push(BOOL_VAL(true)); break;

      case OP_EQUAL: {
        Value a = pop();
        Value b = pop();
        push(BOOL_VAL(Value_equals(a, b)));
        break;
      }
      case OP_GREATER: BINARY_OP(BOOL_VAL, >); break;
      case OP_LESS:    BINARY_OP(BOOL_VAL, <); break;

      case OP_NEGATE: { 
        if (!IS_NUMBER(peek(0))) {
          runtime_error(&vm, "Operand must be a number.");
          return INTERPRET_RUNTIME_ERROR;
        }
        push(NUMBER_VAL(-AS_NUMBER(pop())));
        break;
      }

      case OP_ADD: BINARY_OP(NUMBER_VAL, +); break;
      case OP_SUB: BINARY_OP(NUMBER_VAL, -); break;
      case OP_MUL: BINARY_OP(NUMBER_VAL, *); break;
      case OP_DIV: BINARY_OP(NUMBER_VAL, /); break;
      case OP_NOT: push(BOOL_VAL(is_falsey(pop()))); break;
    }
  }

  #undef READ_BYTE
  #undef READ_CONSTANT
  #undef READ_CONSTANT_LONG
}

InterpretResult VM_interpret(const char *source) {
  Chunk chunk;
  Chunk_init(&chunk);

  if (!compile(source, &chunk)) {
    Chunk_free(&chunk);
    return INTERPRET_COMPILE_ERROR;
  }

  vm.chunk = &chunk;
  vm.ip = vm.chunk->code;

  InterpretResult result = run();

  Chunk_free(&chunk);
  return result;
}

void VM_free() {}

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

  reset_stack();
}

static void push(Value value) {
  *vm.stack_top = value;
  vm.stack_top++;
}

static Value pop() {
  vm.stack_top--;
  return *vm.stack_top;
}

static Value peek(size_t depth) {
  return vm.stack_top[-1 - depth];
}

static void reset_stack() {
  vm.stack_top = vm.stack;
}

