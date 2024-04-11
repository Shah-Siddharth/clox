#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"

// opcodes for different instructions
typedef enum {
  OP_RETURN,
} OpCode;

// chunks are dynamic arrays that will store bytecodes
typedef struct {
  int capacity;
  int count;
  uint8_t* code;
} Chunk;

void freeChunk(Chunk* chunk);
void initChunk(Chunk* chunk);
void writeChunk(Chunk* chunk, uint8_t byte);

#endif
