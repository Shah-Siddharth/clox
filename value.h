#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef struct Object Object;
typedef struct StringObject StringObject;

typedef enum
{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER,
    VAL_OBJECT
} ValueType;

typedef struct
{
    ValueType type;
    union
    {
        bool boolean;
        double number;
        Object *object;
    } as;
} Value;

// macros to convert C value to Lox value
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})
#define OBJECT_VAL(obj) ((Value){VAL_OBJECT, {.object = (Object *)obj}})

// macros to check Value's type. Return 'true' if type matches expected type
#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

// macros to unpack Lox value and get C value
#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJECT(value) ((value).as.object)

// dynamic array of values to store all constants
typedef struct
{
    int capacity;
    int count;
    Value *values;
} ValueArray;

bool valuesEqual(Value a, Value b);

void freeValueArray(ValueArray *array);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);

void printValue(Value value);

#endif
