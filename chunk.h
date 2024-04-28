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
    int *lines;           // dynamic array to store line no. for bytecodes
    ValueArray constants; // dynamic array to store all constants
} Chunk;

void freeChunk(Chunk *chunk);
void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t byte, int lineNumber);

// helper function to add constant to constant pool of chunk
int addConstant(Chunk *chunk, Value value);

#endif
