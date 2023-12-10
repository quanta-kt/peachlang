#include "chunk.h"
#include "memory.h"
#include "value.h"

void Chunk_init(Chunk* chunk) {
  chunk->code = NULL;
  chunk->count = 0;
  chunk->capacity = 0;

  chunk->line_capacity = 0;
  chunk->line_count = 0;
  chunk->lines = NULL;

  ValueArray_init(&chunk->constants);
}

void Chunk_write(Chunk* chunk, uint8_t byte, size_t line) {
  if (chunk->capacity < chunk->count + 1) {
    size_t old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  chunk->count++;

  if (chunk->line_count > 0 &&
      chunk->lines[chunk->line_count - 1].line == line) {
    return;
  }

  if (chunk->line_capacity < chunk->line_count + 1) {
    size_t old_capacity = chunk->line_capacity;
    chunk->line_capacity = GROW_CAPACITY(old_capacity);
    chunk->lines = GROW_ARRAY(LineStart, chunk->lines, old_capacity, chunk->line_capacity);
  }

  LineStart * line_start = &chunk->lines[chunk->line_count++];
  line_start->offset = chunk->count - 1;
  line_start->line = line;
}

size_t Chunk_get_line(Chunk* chunk, size_t instruction) {
  size_t start = 0;
  size_t end = chunk->line_count - 1;


  for (;;) {
    size_t mid = ((end - start) / 2) + start;
    LineStart* line = &chunk->lines[mid];

    if (instruction < line->offset) {
      end = mid - 1;
    } else if (mid == chunk->line_count - 1 || 
        instruction < chunk->lines[mid + 1].offset) {

      return line->line;
    } else {
      start = mid + 1;
    }
  }
}

void Chunk_free(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
  FREE_ARRAY(LineStart, chunk->lines, chunk->line_capacity);
  ValueArray_free(&chunk->constants);

  Chunk_init(chunk);
}

size_t Chunk_add_constant(Chunk* chunk, Value value) {
  ValueArray_write(&chunk->constants, value);
  return chunk->constants.count - 1;
}

