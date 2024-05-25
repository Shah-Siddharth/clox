#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "value.h"

#define VM_STACK_MAX 256

typedef struct
{
    Chunk *chunk;
    uint8_t *instructionPointer; // points to the next instruction
    Value stack[VM_STACK_MAX];
    Value *stackTop; // points past the last item of the stack
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

void initVM();
InterpretResult interpret(Chunk *chunk);
void freeVM();

void pushToStack(Value value);
Value popFromStack();

#endif