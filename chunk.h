#ifndef clox_chunk_h
#define clox_chunk_h

#include "common.h"
#include "value.h"

// opcodes for different instructions
typedef enum
{
    OP_CONSTANT,
    OP_NIL,
    OP_FALSE,
    OP_TRUE,

    // binary arithmetic operators
    OP_ADD,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,

    // comparison operators
    OP_EQUAL,
    OP_GREATER,
    OP_LESS,

    OP_NOT,
    OP_NEGATE,
    OP_PRINT,
    OP_POP,
    OP_GET_LOCAL,
    OP_SET_LOCAL,
    OP_DEFINE_GLOBAL,
    OP_GET_GLOBAL,
    OP_SET_GLOBAL,
    OP_JUMP,
    OP_JUMP_IF_FALSE,
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
