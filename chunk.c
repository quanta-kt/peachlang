#include <stdint.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void Chunk_init(Chunk* chunk) {
  chunk->code = NULL;
  chunk->count = 0;
  chunk->capacity = 0;
  
  chunk->lines = NULL;

  ValueArray_init(&chunk->constants);
}

void Chunk_write(Chunk* chunk, uint8_t byte, size_t line) {
  if (chunk->capacity < chunk->count + 1) {
    size_t old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
    chunk->lines = GROW_ARRAY(size_t, chunk->lines, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->lines[chunk->count] = line;
  chunk->count++;
}

void Chunk_free(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(size_t, chunk->lines, chunk->capacity);

  ValueArray_free(&chunk->constants);

  Chunk_init(chunk);
}

size_t Chunk_add_constant(Chunk* chunk, Value value) {
  ValueArray_write(&chunk->constants, value);
  return chunk->constants.count - 1;
}

