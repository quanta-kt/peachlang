#ifndef peach_chunk_h 
#define peach_chunk_h 

#include "common.h"
#include "value.h"

typedef enum {
  OP_LOAD_CONST,
  OP_RETURN,
} OpCode;

typedef struct {
  size_t line;
  uint8_t times;
} LineInfoItem;

typedef struct {
  size_t capacity;
  size_t size;
  LineInfoItem* items;
} LineInfo;

void LineInfo_init(LineInfo* line_info);

void LineInfo_add(LineInfo* line_info, size_t line);

size_t LineInfo_get(LineInfo *line_info, size_t at);

void LineInfo_free(LineInfo* line_info);


typedef struct {
  size_t count;
  size_t capacity;
  uint8_t* code;
  ValueArray constants;

  LineInfo lines;
} Chunk;

void Chunk_init(Chunk* chunkl);

void Chunk_write(Chunk* chunk, uint8_t byte, size_t line);

size_t Chunk_add_constant(Chunk *chunk, Value value);

void Chunk_free(Chunk* chunk);

#endif /* ifndef peach_chunk_h */

