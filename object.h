#ifndef clox_object_h
#define clox_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

// denotes the type of an object
typedef enum
{
    OBJECT_CLOSURE,
    OBJECT_FUNCTION,
    OBJECT_NATIVE,
    OBJECT_STRING
} ObjectType;

// acts like a 'base class / struct' for all objects
struct Object
{
    ObjectType type;
    struct Object *next; // points to the next object in the linked list
};

// function objects
typedef struct
{
    Object object;
    int arity;
    Chunk chunk;
    StringObject *name;
} FunctionObject;

// NativeFunction is a pointer to a function that returns Value
typedef Value (*NativeFunction)(int argCount, Value *args);

typedef struct
{
    Object object;
    NativeFunction function;
} NativeObject;

// string objects
struct StringObject
{
    Object object;
    int length;
    char *chars;
    uint32_t hash; // since strings are immutable, we can calculate and store hash up front
};

typedef struct
{
    Object object;
    FunctionObject *function;
} ClosureObject;

ClosureObject *newClosure(FunctionObject *function);

FunctionObject *newFunction();

NativeObject *newNative(NativeFunction function);

StringObject *copyString(const char *chars, int length);
StringObject *takeString(char *chars, int length);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type)
{
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

// macro that returns the type of object
#define OBJ_TYPE(value) (AS_OBJECT(value)->type)

#define IS_CLOSURE(value) isObjectType(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) isObjectType(value, OBJECT_FUNCTION)
#define IS_NATIVE(value) isObjectType(value, OBJECT_NATIVE);
#define IS_STRING(value) isObjectType(value, OBJECT_STRING)

#define AS_CLOSURE(value) ((ClosureObject *)AS_OBJECT(value))

// takes pointer to a value of type function and returns FunctionObject* pointer
#define AS_FUNCTION(value) ((FunctionObject *)AS_OBJECT(value))

// takes a pointer to a value of type NativeObject and extracts the C function pointer from it
#define AS_NATIVE(value) (((NativeObject *)AS_OBJECT(value))->function);

// takes pointer to a value of type string and returns StringObject* pointer
#define AS_STRING(value) ((StringObject *)AS_OBJECT(value))

// takes pointer to object of type string and returns the character array of the object
#define AS_CSTRING(value) (((StringObject *)AS_OBJECT(value))->chars)

#endif