#include <stdio.h>

#include "chunk.h"
#include "debug.h"

static size_t constant_instruction(const char* name, const Chunk* chunk, const size_t offset);
static size_t simple_instruction(const char* name, int offset);

void disassemble_chunk(Chunk *chunk, const char *name) {
  printf("== %s ==\n", name);

  for (size_t offset = 0; offset < chunk->count;) {
    offset = disassemble_instruction(chunk, offset);
  }
}

size_t disassemble_instruction(Chunk* chunk, size_t offset) {
  printf("%04zu ", offset);

  if (offset > 0 && chunk->lines[offset] == chunk->lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4zu ", chunk->lines[offset]);
  }

  uint8_t instruction = chunk->code[offset];

  switch (instruction) {  
    case OP_LOAD_CONST:
      return constant_instruction("OP_LOAD_CONST", chunk, offset);

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

