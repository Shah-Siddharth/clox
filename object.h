#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "value.h"

// denotes the type of an object
typedef enum
{
    OBJECT_STRING
} ObjectType;

// acts like a 'base class / struct' for all objects
struct Object
{
    ObjectType type;
};

// macro that returns the type of object
#define OBJ_TYPE(value) (AS_OBJ(value)->type)

#endif