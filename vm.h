#ifndef peach_vm_h
#define peach_vm_h

#include "chunk.h"
#include "value.h"

#define STACK_MAX 1024 

typedef struct {
  // The code to execute
  Chunk* chunk;

  // The *next* instruction to interpret
  uint8_t* ip;

  // The VM stack
  Value stack[STACK_MAX];

  // points at the array element just past the element
  // containing the top value on the stack
  // i.e top + 1
  Value* stack_top;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void VM_init();

InterpretResult VM_interpret(const char* source);

void VM_free();

#endif // !peach_vm_h

