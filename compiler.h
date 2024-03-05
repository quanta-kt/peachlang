#ifndef peach_compiler_h 
#define peach_compiler_h

#include "vm.h"

ObjectFunction* compile(VM* vm, const char* source);

#endif // !peach_compiler_h

