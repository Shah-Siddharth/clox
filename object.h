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
    struct Object *next; // points to the next object in the linked list
};

// string objects
struct StringObject
{
    Object object;
    int length;
    char *chars;
    uint32_t hash; // since strings are immutable, we can calculate and store hash up front
};

StringObject *copyString(const char *chars, int length);
StringObject *takeString(char *chars, int length);
void printObject(Value value);

static inline bool isObjectType(Value value, ObjectType type)
{
    return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

// macro that returns the type of object
#define OBJ_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value) isObjectType(value, OBJECT_STRING)

// takes pointer to object of type string and returns StringObject* pointer
#define AS_STRING(value) ((StringObject *)AS_OBJECT(value))

// takes pointer to object of type string and returns the character array of the object
#define AS_CSTRING(value) (((StringObject *)AS_OBJECT(value))->chars)

#endif