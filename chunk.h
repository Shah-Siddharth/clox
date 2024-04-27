#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// opcodes for different instructions
typedef enum
{
    OP_CONSTANT,
    OP_RETURN,
} OpCode;

// chunks are dynamic arrays that will store bytecodes
typedef struct
{
    int capacity;
    int count;
    uint8_t *code;
    ValueArray constants; // a dynamic array to store all constants
} Chunk;

void freeChunk(Chunk *chunk);
void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte);

// helper function to add constant to constant pool of chunk
int addConstant(Chunk *chunk, Value value);

#endif
