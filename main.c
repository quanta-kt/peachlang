#include <stdio.h>

#include "common.h"
#include "chunk.h"
#include "debug.h"
#include "vm.h"

int main(int argc, char *argv[]) {
  VM_init();

  Chunk chunk;

  Chunk_init(&chunk);

  size_t constant = Chunk_add_constant(&chunk, 1);

  // 1 + 2 * 3 - 4 / -5
  Chunk_write_constant(&chunk, 1, 1);

  Chunk_write_constant(&chunk, 2, 1);
  Chunk_write_constant(&chunk, 3, 1);
  Chunk_write(&chunk, OP_MUL, 1);

  Chunk_write_constant(&chunk, 4, 1);
  Chunk_write_constant(&chunk, 5, 1);
  Chunk_write(&chunk, OP_NEGATE, 1);
  Chunk_write(&chunk, OP_DIV, 1);

  Chunk_write(&chunk, OP_SUB, 1);
  Chunk_write(&chunk, OP_ADD, 1);

  Chunk_write(&chunk, OP_RETURN, 1);

  // 3 - 2 - 1
  Chunk_write_constant(&chunk, 3, 1);
  Chunk_write_constant(&chunk, 2, 1);
  Chunk_write(&chunk, OP_SUB, 1);

  Chunk_write_constant(&chunk, 1, 1);
  Chunk_write(&chunk, OP_SUB, 1);
  Chunk_write(&chunk, OP_RETURN, 1);

  // 1 + 2 * 3
  Chunk_write_constant(&chunk, 2, 1);
  Chunk_write_constant(&chunk, 3, 1);
  Chunk_write(&chunk, OP_MUL, 1);

  Chunk_write_constant(&chunk, 1, 1);
  Chunk_write(&chunk, OP_ADD, 1);
  Chunk_write(&chunk, OP_RETURN, 1);

  // 1 * 2 + 3
  Chunk_write_constant(&chunk, 1, 1);
  Chunk_write_constant(&chunk, 2, 1);
  Chunk_write(&chunk, OP_MUL, 1);

  Chunk_write_constant(&chunk, 3, 1);
  Chunk_write(&chunk, OP_ADD, 1);

  Chunk_write(&chunk, OP_RETURN, 1);

  VM_interpret(&chunk);

  VM_free();
  Chunk_free(&chunk);

  return 0;
}

