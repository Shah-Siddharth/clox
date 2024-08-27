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

    // The newly allocated object is added to the beginning of the singly linked list.
    object->next = vm.objects;
    vm.objects = object;

    return object;
}

static StringObject *allocateString(char *chars, int length, uint32_t hash)
{
    StringObject *string = ALLOCATE_OBJECT(StringObject, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    return string;
}

// hashes a string using the "FNV-1a" hash function
static uint32_t hashString(const char *key, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)key[i];
        hash *= 16777619;
    }
}

// allocates array just big enough for the string.
// then copies the characters from the lexeme to the array
StringObject *copyString(const char *chars, int length)
{
    uint32_t hash = hashString(chars, length);
    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';
    return allocateString(heapChars, length, hash);
}

StringObject *takeString(char *chars, int length)
{
    uint32_t hash = hashString(chars, length);
    return allocateString(chars, length, hash);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJECT_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}