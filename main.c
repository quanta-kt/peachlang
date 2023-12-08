#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, char *argv[]) {
  Chunk chunk;

  Chunk_init(&chunk);

  size_t constant = Chunk_add_constant(&chunk, 12);

  Chunk_write(&chunk, OP_LOAD_CONST, 123);
  Chunk_write(&chunk, constant, 123);

  Chunk_write(&chunk, OP_RETURN, 123);

  disassemble_chunk(&chunk, "test chunk");

  Chunk_free(&chunk);

  return 0;
}

