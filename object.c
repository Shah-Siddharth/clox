#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJECT(type, objectType) \
    (type *)allocateObject(sizeof(type), objectType)

// allocates an object of given size and type on the heap
static Object *allocateObject(size_t size, ObjectType type)
{
    Object *object = (Object *)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

static StringObject *allocateString(char *chars, int length)
{
    StringObject *string = ALLOCATE_OBJECT(StringObject, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

// allocates array just big enough for the string.
// then copies the characters from the lexeme to the array
StringObject *copyString(const char *chars, int length)
{
    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length);
}