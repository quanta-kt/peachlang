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
  Chunk_write(&chunk, OP_LOAD_CONST, 123);
  Chunk_write(&chunk, constant, 123);

  Chunk_write(&chunk, OP_RETURN, 124);
  Chunk_write(&chunk, OP_RETURN, 124);
  Chunk_write(&chunk, OP_RETURN, 124);
  Chunk_write(&chunk, OP_RETURN, 124);

  for (double i = 0; i < 260; i++) {
    Chunk_write_constant(&chunk, i, i + 125);
  }

  Chunk_write(&chunk, OP_LOAD_CONST, 125 + 260);
  Chunk_write(&chunk, constant, 125 + 260);

  Chunk_write(&chunk, OP_RETURN, 125 + 260);

  disassemble_chunk(&chunk, "test chunk");

  Chunk_free(&chunk);

  return 0;
}

