#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

VM vm;

static void resetVMStack()
{
    vm.stackTop = vm.stack; // initially points to beginning of the array
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.instructionPointer - vm.chunk->code - 1;
    int line = vm.chunk->lines[instruction];
    fprintf(stderr, "[line %d] in script\n", line);
    resetVMStack();
}

void initVM()
{
    resetVMStack();
}

InterpretResult interpretCode(const char *sourceCode)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compileCode(sourceCode, &chunk))
    {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.instructionPointer = vm.chunk->code;

    InterpretResult result = run();

    freeChunk(&chunk);
    return result;
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

static Value peek(int distance)
{
    return vm.stackTop[-(distance + 1)];
}

static bool isFalsey(Value value)
{
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate()
{
    StringObject *b = AS_STRING(popFromStack());
    StringObject *a = AS_STRING(popFromStack());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    StringObject *result = takeString(chars, length);
    pushToStack(OBJECT_VAL(result));
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.instructionPointer++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define BINARY_OP(valueType, op)                        \
    do                                                  \
    {                                                   \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            runtimeError("Operands must be numbers.");  \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        double b = AS_NUMBER(popFromStack());           \
        double a = AS_NUMBER(popFromStack());           \
        pushToStack(valueType(a op b));                 \
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
        case OP_NIL:
            pushToStack(NIL_VAL);
            break;
        case OP_FALSE:
            pushToStack(BOOL_VAL(false));
            break;
        case OP_TRUE:
            pushToStack(BOOL_VAL(true));
            break;

        case OP_EQUAL:
        {
            Value b = popFromStack();
            Value a = popFromStack();
            pushToStack(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;

        case OP_ADD:
        {
            if (IS_STRING(peek(0)) && IS_NUMBER(peek(1)))
            {
                concatenate();
            }
            else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1)))
            {
                double b = AS_NUMBER(popFromStack());
                double a = AS_NUMBER(popFromStack());
                pushToStack(NUMBER_VAL(a + b));
            }
            else
            {
                runtimeError("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;

        case OP_NOT:
            pushToStack(BOOL_VAL(isFalsey(popFromStack())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0)))
            {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            pushToStack(NUMBER_VAL(-AS_NUMBER(popFromStack())));
            break;
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