#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"
#include "vm.h"

void * reallocate(void* pointer, size_t old_size, size_t new_size) {
  if (new_size > old_size) {
    #ifdef DEBUG_STRESS_GC
      gc();
    #endif
  }

  if (new_size == 0) {
    free(pointer);
    return NULL;
  }

  void* result = realloc(pointer, new_size);

  if (result == NULL) {
    fprintf(stderr, "peach: out of memory.");
  }

  return result;
}

void free_objects(Object *head) {
  Object* object = head;

  while (object != NULL) {
    Object* next = object->next;
    free_object(object);
    object = next;
  }
}

void free_object(Object* object) {
  #ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void*) object, object->type);
  #endif

  switch (object->type) {
    case OBJ_STRING: {
      ObjectString* str = (ObjectString*) object;
      FREE_ARRAY(char, str->chars, str->length + 1);
      FREE(ObjectString, object);
    }
    case OBJ_UPVALUE: {
      FREE(ObjectUpvalue, object);
      break;
    }
    case OBJ_FUNCTION: {
      ObjectFunction* function = (ObjectFunction*)object;
      Chunk_free(&function->chunk);
      FREE(ObjectFunction, object);
      break;
    }
    case OBJ_CLOSURE: {
      ObjectClosure* closure = (ObjectClosure*) object;
      FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalue_count);
      FREE(ObjectClosure, closure);
      break;
    }
    case OBJ_NATIVE_FN: {
      FREE(ObjectFunction, object);
      break;
    }
  }
}

static void mark_roots() {

}

void gc() {
  #ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
  #endif

  mark_roots();

  #ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
  #endif
}
