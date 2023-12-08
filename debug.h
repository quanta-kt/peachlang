#ifndef peach_debug_h 
#define peach_debug_h

#include "common.h"
#include "chunk.h"

void disassemble_chunk(Chunk* chunk, const char* name);

size_t disassemble_instruction(Chunk* chunk, size_t offset);

#endif // !peach_debug_h

