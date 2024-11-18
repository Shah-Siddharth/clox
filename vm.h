#ifndef clox_vm_h
#define clox_vm_h

#include "chunk.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    FunctionObject *function;
    uint8_t *instructionPointer;
    Value *slots; // The first slot on the VM's value stack that the function can use
} CallFrame;

typedef struct
{
    CallFrame frames[FRAMES_MAX];
    int frameCount; // current height of callframe stack. i.e., no. of ongoing function calls

    Value stack[STACK_MAX];
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