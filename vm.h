#ifndef peach_vm_h
#define peach_vm_h

#include "object.h"
#include "chunk.h"
#include "value.h"
#include "table.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct {
  ObjectClosure* closure;
  uint8_t* ip;
  Value* slots;
} CallFrame;

typedef struct {
  CallFrame frames[FRAMES_MAX];
  int frame_count;

  // The VM stack
  Value stack[STACK_MAX];

  // points at the array element just past the element
  // containing the top value on the stack
  // i.e top + 1
  Value* stack_top;

  Table globals;

  Object* objects;

  Table strings;

} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void VM_init(VM* vm);

InterpretResult VM_interpret(VM* vm, const char* source);

/**
 * Retrives an interned string from the VM. If one does not already exists, it will
 * be created. `dest` will be updated to point to that object regardless.
 *
 * Returns true if a new string was created.
 */
bool VM_get_intern_str(VM* vm, const char* chars, size_t length, ObjectString** dest);

/**
 * Retrives an interned string from the VM. If one does not already exists, it will
 * be created. `dest` will be updated to point to that object regardless.
 *
 * This function differs from `VM_get_intern_str` in that it will take the ownership
 * of the supplied string instead of copying it; regardless of whether or not a new
 * string was created.
 *
 * Returns true if a new string was created.
 */
bool VM_get_intern_str_take(VM* vm, char* chars, size_t length, ObjectString** dest);

void VM_free(VM* vm);

#endif // !peach_vm_h

