#ifndef peach_compiler_h 
#define peach_compiler_h

#include "vm.h"

bool compile(VM* vm, const char* source, Chunk* chunk);

#endif // !peach_compiler_h

