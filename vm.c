#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "value.h"
#include "vm.h" 
#include "debug.h"

#include <stdio.h>

VM vm;

static void push(Value value);

static Value pop();

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
  
  #define BINARY_OP(op) \
    do { \
      double b = pop(); \
      double a = pop(); \
      push(a op b); \
    } while(false) \

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

      case OP_NEGATE: {
        push(-pop());
        break;
      }

      case OP_ADD: BINARY_OP(+); break;
      case OP_SUB: BINARY_OP(-); break;
      case OP_MUL: BINARY_OP(*); break;
      case OP_DIV: BINARY_OP(/); break;
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

static void push(Value value) {
  *vm.stack_top = value;
  vm.stack_top++;
}

static Value pop() {
  vm.stack_top--;
  return *vm.stack_top;
}

static void reset_stack() {
  vm.stack_top = vm.stack;
}

