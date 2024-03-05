#include "chunk.h"
#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h" 
#include "debug.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static void concatenate(VM* vm);
static bool is_falsey(Value value);
static void runtime_error(VM* vm, const char* fmt, ...);
static void VM_define_native(VM* vm, const char* name, NativeFn fn);
static void push(VM* vm, Value value);
static Value pop(VM* vm);
static Value peek(VM* vm, size_t depth);
static void reset_stack(VM* vm);
bool call_value(VM* vm, Value callee, uint8_t arg_count);

static Value native_clock();

void VM_init(VM* vm) {
  Table_init(&vm->globals);
  Table_init(&vm->strings);
  reset_stack(vm);
  vm->objects = NULL;

  VM_define_native(vm, "clock", native_clock);
}

static InterpretResult run(VM* vm) {
  CallFrame* frame = &vm->frames[vm->frame_count - 1];

  #define READ_BYTE() (*(frame->ip++))

  #define READ_SHORT() ( \
    READ_BYTE() \
    | (READ_BYTE() << 8) \
  )

  #define READ_LONG() ( \
    READ_BYTE() \
    | (READ_BYTE() << 8) \
    | (READ_BYTE() << 16) \
  )

  #define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])
  #define READ_CONSTANT_LONG() ( \
    frame->function->chunk.constants.values[READ_LONG()])
  
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

    if (vm->stack >= vm->stack_top) {
      printf("<empty stack>");
    }

    for (Value* slot = vm->stack; slot < vm->stack_top; slot++) {
      printf("[ ");
      Value_print(*slot);
      printf(" ]");
    }
    printf("\n");

    disassemble_instruction(&frame->function->chunk, (size_t)(frame->ip - frame->function->chunk.code));
    #endif /* ifdef DEBUG_TRACE_EXECUTION */

    uint8_t instruction;

    switch (instruction = READ_BYTE()) {
      case OP_PRINT: {
        Value_print(pop(vm));
        printf("\n");
        break;
      }

      case OP_POP: {
        pop(vm);
        break;
      }

      case OP_RETURN: {
        Value result = pop(vm);
        vm->frame_count--;

        if (vm->frame_count == 0) {
          pop(vm);
          return INTERPRET_OK;
        }

        vm->stack_top = frame->slots;
        push(vm, result);
        frame = &vm->frames[vm->frame_count - 1];
        break;
      }

      case OP_DEF_GLOBAL: {
        ObjectString* name = AS_STRING(READ_CONSTANT());
        Table_set(&vm->globals, name, peek(vm, 0));
        pop(vm);
        break;
      }

      case OP_DEF_GLOBAL_LONG: {
        ObjectString* name = AS_STRING(READ_CONSTANT_LONG());
        Table_set(&vm->globals, name, peek(vm, 0));
        pop(vm);
        break;
      }

      case OP_GET_GLOBAL: {
        ObjectString* name = AS_STRING(READ_CONSTANT());
        Value value;

        if (!Table_get(&vm->globals, name, &value)) {
          runtime_error(vm, "Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(vm, value);
        break;
      }

      case OP_GET_GLOBAL_LONG: {
        ObjectString* name = AS_STRING(READ_CONSTANT_LONG());
        Value value;

        if (!Table_get(&vm->globals, name, &value)) {
          runtime_error(vm, "Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        push(vm, value);
        break;
      }

      case OP_SET_GLOBAL: {
        ObjectString* name = AS_STRING(READ_CONSTANT());

        if (Table_set(&vm->globals, name, peek(vm, 0))) {
          Table_delete(&vm->globals, name);
          runtime_error(vm, "Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }

      case OP_SET_GLOBAL_LONG: {
        ObjectString* name = AS_STRING(READ_CONSTANT_LONG());

        if (Table_set(&vm->globals, name, peek(vm, 0))) {
          Table_delete(&vm->globals, name);
          runtime_error(vm, "Undefined variable '%s'.", name->chars);
          return INTERPRET_RUNTIME_ERROR;
        }

        break;
      }

      case OP_GET_LOCAL: {
        size_t slot = READ_BYTE();
        push(vm, frame->slots[slot]);
        break;
      }
      case OP_GET_LOCAL_LONG: {
        size_t slot = READ_LONG();
        push(vm, frame->slots[slot]);
        break;
      }
      case OP_SET_LOCAL: {
        size_t slot = READ_BYTE();
        frame->slots[slot] = peek(vm, 0);
        break;
      }
      case OP_SET_LOCAL_LONG: {
        size_t slot = READ_LONG();
        frame->slots[slot] = peek(vm, 0);
        break;
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

      case OP_JUMP_IF_FALSE: {
        uint16_t offset = READ_SHORT();   
        if (is_falsey(peek(vm, 0))) frame->ip += offset;
        break;
      }

      case OP_JUMP: {
        uint16_t offset = READ_SHORT();   
        frame->ip += offset;
        break;
      }

      case OP_LOOP: {
        uint16_t offset = READ_SHORT();
        frame->ip -= offset;
        break;
      }

      case OP_CALL: {
        uint8_t arg_count = READ_BYTE();
        if (!call_value(vm, peek(vm, arg_count), arg_count)) {
          return INTERPRET_RUNTIME_ERROR;
        }

        frame = &vm->frames[vm->frame_count - 1];
        break;
      }
    }
  }

  #undef READ_BYTE
  #undef READ_SHORT 
  #undef READ_CONSTANT
  #undef READ_CONSTANT_LONG
}

static bool call(VM* vm, ObjectFunction* fn, int arg_count) {
  if (arg_count != fn->arity) {
    runtime_error(vm, "Expected %d arguments but got %d.", fn->arity, arg_count);
    return false;
  }

  if (vm->frame_count == FRAMES_MAX) {
    runtime_error(vm, "Stack overflow");
    return false;
  }

  CallFrame* frame = &vm->frames[vm->frame_count++];
  frame->function = fn;
  frame->ip = fn->chunk.code;
  frame->slots = vm->stack_top - arg_count - 1;
  return true;
}

bool call_value(VM* vm, Value callee, uint8_t arg_count) {
  if (IS_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
      case OBJ_FUNCTION: 
        return call(vm, AS_FUNCTION(callee), arg_count);

      case OBJ_NATIVE_FN: {
        NativeFn fn = AS_NATIVE_FN(callee);
        Value result = fn(arg_count, vm->stack_top - arg_count);
        vm->stack_top -= arg_count + 1;
        push(vm, result);
        return true;
      }
      default:
        break; // Non-callable object type.
    }
  }

  runtime_error(vm, "Can only call functions and classes.");
  return false;
}

InterpretResult VM_interpret(VM* vm, const char *source) {
  ObjectFunction* fn = compile(vm, source);
  if (fn == NULL) return INTERPRET_COMPILE_ERROR;

  push(vm, OBJECT_VAL(fn));
  call(vm, fn, 0);

  return run(vm);
}

bool VM_get_intern_str(VM* vm, const char* chars, size_t length, ObjectString** dest) {
  uint32_t hash = string_hash(chars, length);

  ObjectString* str = Table_find_str(&vm->strings, chars, length);
  bool create = str == NULL;

  if (create) {
    str = ObjectString_copy(chars, length);
    Table_set(&vm->strings, str, NIL_VAL);
  }

  *dest = str;
  return create;
}

bool VM_get_intern_str_take(VM* vm, char* chars, size_t length, ObjectString** dest) {
  uint32_t hash = string_hash(chars, length);

  ObjectString* str = Table_find_str(&vm->strings, chars, length);
  bool create = str == NULL;

  if (create) {
    str = ObjectString_take(chars, length);
    Table_set(&vm->strings, str, NIL_VAL);
  } else {
    FREE_ARRAY(char, chars, length + 1);
  }

  *dest = str;
  return create;
}

void VM_free(VM* vm) {
  Table_free(&vm->strings);
  Table_free(&vm->globals);
  free_objects(vm->objects);
}

static void concatenate(VM* vm) {
  ObjectString* b = AS_STRING(pop(vm));
  ObjectString* a = AS_STRING(pop(vm));
  size_t length = a->length + b->length;

  char* str = ALLOCATE(char, length + 1);

  memcpy(str, a->chars, a->length);
  memcpy(str + a->length, b->chars, b->length);
  str[length] = '\0';

  ObjectString* dest;
  VM_get_intern_str_take(vm, str, length, &dest);
  push(vm, OBJECT_VAL(dest));
}

static bool is_falsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void runtime_error(VM* vm, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);

  fputs("\n", stderr);

  for (int i = vm->frame_count - 1; i >= 0; i--) {
    CallFrame* frame = &vm->frames[i];
    ObjectFunction* function = frame->function;
    size_t instruction = frame->ip - frame->function->chunk.code - 1;
    size_t line = Chunk_get_line(&frame->function->chunk, instruction);
    fprintf(stderr, "[line %zu] in script\n", line);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }

  reset_stack(vm);
}

static void VM_define_native(VM* vm, const char* name, NativeFn fn) {
  ObjectString* str;
  VM_get_intern_str(vm, name, strlen(name), &str);

  push(vm, OBJECT_VAL(str));
  push(vm, OBJECT_VAL(ObjectNativeFn_create(fn)));
  Table_set(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
  pop(vm);
  pop(vm);
}

static Value native_clock() {
  return NUMBER_VAL((double) clock() / CLOCKS_PER_SEC);
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
  vm->frame_count = 0;
}

