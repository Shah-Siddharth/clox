#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

void initVM()
{
}

InterpretResult interpret(Chunk *chunk)
{
    vm.chunk = chunk;
    vm.instructionPointer = vm.chunk->code;
    return run();
}

void freeVM()
{
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.instructionPointer++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        disassembleInstruction(vm.chunk, (int)(vm.instructionPointer - vm.chunk->code));
#endif

        // read byte pointed by IP and advance IP
        uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            printValue(constant);
            printf("\n");
            break;
        }
        case OP_RETURN:
        {
            return INTERPRET_OK;
        }

        default:
            break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
}