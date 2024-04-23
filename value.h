#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef double Value;

// dynamic array of values to store all constants
typedef struct
{
    int capacity;
    int count;
    Value *values;
} ValueArray;

void freeValueArray(ValueArray *array);
void initValueArray(ValueArray *array);
void writeValueArray(ValueArray *array, Value value);

#endif
