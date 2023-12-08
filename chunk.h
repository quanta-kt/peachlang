#ifndef peach_chunk_h 
#define peach_chunk_h 

#include "common.h"
#include "value.h"

typedef enum {
  OP_LOAD_CONST,
  OP_RETURN,
} OpCode;

typedef struct {
  size_t count;
  size_t capacity;
  uint8_t* code;
  ValueArray constants;

  size_t* lines;
} Chunk;

void Chunk_init(Chunk* chunkl);

void Chunk_write(Chunk* chunk, uint8_t byte, size_t line);

size_t Chunk_add_constant(Chunk *chunk, Value value);

void Chunk_free(Chunk* chunk);

#endif /* ifndef peach_chunk_h */

