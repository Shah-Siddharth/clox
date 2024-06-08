#ifndef clox_compiler_h
#define clox_compiler_h

#include "vm.h"

// compile source code and fill the chunk with bytecode
void compileCode(const char *sourceCode, Chunk *chunk);

#endif