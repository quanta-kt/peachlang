#include <stdint.h>

#include "chunk.h"
#include "memory.h"
#include "value.h"

void LineInfo_init(LineInfo* line_info) {
  line_info->capacity = 0;
  line_info->size = 0;
  line_info->items = NULL;
}

void LineInfo_free(LineInfo* line_info) {
  FREE_ARRAY(LineInfoItem, line_info->items, line_info->capacity);
}

void LineInfo_add(LineInfo* line_info, size_t line) {
  if (line_info->size > 0 && line_info->items[line_info->size - 1].line == line) {
    line_info->items[line_info->size - 1].times++;
    return;
  } 

  if (line_info->capacity < line_info->size + 1) {
    size_t old_capacity = line_info->capacity;
    line_info->capacity = GROW_CAPACITY(old_capacity); 

    line_info->items = GROW_ARRAY(LineInfoItem, line_info->items, old_capacity, line_info->capacity);
  }

  LineInfoItem new = {
    .times = 1,
    .line = line,
  };

  line_info->items[line_info->size] = new;
  line_info->size++;
}

size_t LineInfo_get(LineInfo *line_info, size_t at) {
  size_t curr = 0;

  for (size_t i = 0; i < line_info->size; i++) {
    size_t curr_end = curr + line_info->items[i].times - 1;

    if (at >= curr && at <= curr_end) {
      return line_info->items[i].line;
    }

    curr = curr_end + 1;
  }

  return 0;
}

void Chunk_init(Chunk* chunk) {
  chunk->code = NULL;
  chunk->count = 0;
  chunk->capacity = 0;
  
  LineInfo_init(&chunk->lines);
  ValueArray_init(&chunk->constants);
}

void Chunk_write(Chunk* chunk, uint8_t byte, size_t line) {
  if (chunk->capacity < chunk->count + 1) {
    size_t old_capacity = chunk->capacity;
    chunk->capacity = GROW_CAPACITY(old_capacity);
    chunk->code = GROW_ARRAY(uint8_t, chunk->code, old_capacity, chunk->capacity);
  }

  chunk->code[chunk->count] = byte;
  LineInfo_add(&chunk->lines, line);
  chunk->count++;
}

void Chunk_free(Chunk* chunk) {
  FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);

  ValueArray_free(&chunk->constants);
  LineInfo_free(&chunk->lines);

  Chunk_init(chunk);
}

size_t Chunk_add_constant(Chunk* chunk, Value value) {
  ValueArray_write(&chunk->constants, value);
  return chunk->constants.count - 1;
}

