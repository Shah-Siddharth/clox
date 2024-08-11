#ifndef clox_compiler_h
#define clox_compiler_h

#include "object.h"
#include "vm.h"

// compile source code and fill the chunk with bytecode
bool compileCode(const char *sourceCode, Chunk *chunk);

#endif