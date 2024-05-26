#include "common.h"
#include "debug.h"
#include "vm.h"

VM vm;

static void resetVMStack()
{
    vm.stackTop = vm.stack; // initially points to beginning of the array
}

void initVM()
{
    resetVMStack();
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

void pushToStack(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value popFromStack()
{
    vm.stackTop--;
    return *vm.stackTop;
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.instructionPointer++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op)              \
    do                             \
    {                              \
        double b = popFromStack(); \
        double a = popFromStack(); \
        pushToStack(a op b);       \
    } while (false)

    for (;;)
    {

// logic to debug the vm (prints stack and disassembles instructions)
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
        {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.instructionPointer - vm.chunk->code));
#endif

        // read byte pointed by IP and advance IP
        uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            pushToStack(constant);
            break;
        }

        case OP_ADD:
            BINARY_OP(+);
            break;
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
            break;

        case OP_NEGATE:
        {
            pushToStack(-popFromStack());
            break;
        }
        case OP_RETURN:
        {
            printValue(popFromStack());
            printf("\n");
            return INTERPRET_OK;
        }

        default:
            break;
        }
    }

#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}