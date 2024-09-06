#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

#define VM_STACK_MAX 256

typedef struct
{
    Chunk *chunk;
    uint8_t *instructionPointer; // points to the next instruction
    Value stack[VM_STACK_MAX];
    Value *stackTop; // points past the last item of the stack
    Table globals;   // stores global variables
    Table strings;   // stores all the strings

    // All objects are stored in a singly linked list. This pointer points to the head of the list.
    Object *objects;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
InterpretResult interpretCode(const char *sourceCode);
void freeVM();

void pushToStack(Value value);
Value popFromStack();

#endif