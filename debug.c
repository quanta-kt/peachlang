#include <stdio.h>

#include "chunk.h"
#include "debug.h"

static size_t constant_instruction(const char* name, const Chunk* chunk, const size_t offset);
static size_t constant_long_instruction(const char* name, const Chunk* chunk, const size_t offset);
static size_t simple_instruction(const char* name, int offset);

void disassemble_chunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (size_t offset = 0; offset < chunk->count;) {
    offset = disassemble_instruction(chunk, offset);
  }
}

size_t disassemble_instruction(Chunk* chunk, size_t offset) {
  printf("%04zu ", offset);

  size_t line = Chunk_get_line(chunk, offset);

  if (offset > 0 && line == Chunk_get_line(chunk, offset - 1)) {
    printf("   | ");
  } else {
    printf("%4zu ", line);
  }

  uint8_t instruction = chunk->code[offset];

  switch (instruction) {  
    case OP_LOAD_CONST:
      return constant_instruction("OP_LOAD_CONST", chunk, offset);

    case OP_LOAD_CONST_LONG:
      return constant_long_instruction("OP_LOAD_CONST_LONG", chunk, offset);

    case OP_RETURN:
      return simple_instruction("OP_RETURN", offset);
    default:
      printf("Unknown opcode: %d\n", instruction);
      return offset + 1;
  }
}

static size_t simple_instruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static size_t constant_instruction(const char* name, const Chunk* chunk, const size_t offset) {
  uint8_t constant = chunk->code[offset + 1];
  printf("%-16s %4d '", name, constant);
  Value_print(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 2;
}

static size_t constant_long_instruction(const char* name, const Chunk* chunk, const size_t offset) {
  const uint8_t a = chunk->code[offset + 1];
  const uint8_t b = chunk->code[offset + 2];
  const uint8_t c = chunk->code[offset + 3];

  const uint32_t constant = a | (b << 8) | (c << 8);

  printf("%-16s %4u '", name, constant);
  Value_print(chunk->constants.values[constant]);
  printf("'\n");

  return offset + 4;
}

