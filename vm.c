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
    vm.frameCount = 0;
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i++)
    {
        CallFrame *frame = &vm.frames[i];
        FunctionObject *function = frame->function;

        // -1 because the IP is already sitting on the next instruction
        size_t instruction = frame->instructionPointer - function->chunk.code - 1;

        fprintf(stderr, "[line %d] in ", function->chunk.lines[instruction]);

        if (function->name == NULL)
        {
            fprintf(stderr, "script\n");
        }
        else
        {
            fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetVMStack();
}

void initVM()
{
    resetVMStack();
    initTable(&vm.globals);
    initTable(&vm.strings);
    vm.objects = NULL;
}

InterpretResult interpretCode(const char *sourceCode)
{
    FunctionObject *function = compileCode(sourceCode);
    if (function == NULL)
    {
        return INTERPRET_COMPILE_ERROR;
    }

    // store top-level function on stack and prepare initial CallFrame to execute it
    pushToStack(OBJECT_VAL(function));
    call(function, 0);

    return run();
}

void freeVM()
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
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

static bool call(FunctionObject *function, int argCount)
{
    // runtime error if user passes too many or too few arguments
    if (argCount != function->arity)
    {
        runtimeError("Expected %d arguments but got %d.", function->arity, argCount);
        return false;
    }

    // runtime error if deep call chain exceeds stack
    if (vm.frameCount == FRAMES_MAX)
    {
        runtimeError("Stack overflow.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->function = function;
    frame->instructionPointer = function->chunk.code;
    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJECT(callee))
    {
        switch (OBJ_TYPE(callee))
        {
        case OBJECT_FUNCTION:
            return call(AS_FUNCTION(callee), argCount);

        default:
            break;
        }
    }
    runtimeError("Can only call functions and classes.");
    return false;
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
    // current topmost callframe
    CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->instructionPointer++)
#define READ_CONSTANT() (frame->function->chunk.constants.values[READ_BYTE()])

// takes the next two bytes from chunk and build a 16 bit unsigned int out of them
#define READ_SHORT() \
    (frame->instructionPointer += 2, (uint16_t)((frame->instructionPointer[-2] << 8) | frame->instructionPointer[-1]))

#define READ_STRING() AS_STRING(READ_CONSTANT())
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
        disassembleInstruction(&frame->function->chunk, (int)(frame->instructionPointer - frame->function->chunk.code));
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
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
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
        case OP_PRINT:
        {
            printValue(popFromStack());
            printf("\n");
            break;
        }
        case OP_POP:
            popFromStack();
            break;
        // locate the value from the stack and push it to the top of the stack.
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            pushToStack(frame->slots[slot]);
            break;
        }
        // take the assigned value from top of the stack and store it in the stack slot.
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_DEFINE_GLOBAL:
        {
            StringObject *name = READ_STRING();
            tableAdd(&vm.globals, name, peek(0));
            popFromStack();
            break;
        }
        case OP_GET_GLOBAL:
        {
            StringObject *name = READ_STRING();
            Value value;
            if (!tableGet(&vm.globals, name, &value))
            {
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            pushToStack(value);
            break;
        }
        case OP_SET_GLOBAL:
        {
            StringObject *name = READ_STRING();
            if (tableAdd(&vm.globals, name, peek(0)))
            {
                tableDelete(&vm.globals, name);
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_JUMP:
        {
            uint16_t offset = READ_SHORT();
            frame->instructionPointer += offset;
            break;
        }
        case OP_JUMP_IF_FALSE:
        {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0)))
                frame->instructionPointer += offset;
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_SHORT();
            frame->instructionPointer -= offset;
            break;
        }
        case OP_CALL:
        {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_RETURN:
        {
            // Exit interpreter
            return INTERPRET_OK;
        }

        default:
            break;
        }
    }

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}