#include <stdlib.h>

#include "memory.h"
#include "vm.h"

static void freeObject(Object *object)
{
  switch (object->type)
  {
  case OBJECT_FUNCTION:
  {
    FunctionObject *function = (FunctionObject *)object;
    freeChunk(&function->chunk);
    FREE(FunctionObject, object);
    break;
  }
  case OBJECT_STRING:
    StringObject *string = (StringObject *)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(StringObject, object);
    break;
  }
}

void freeObjects()
{
  Object *object = vm.objects;
  while (object != NULL)
  {
    Object *next = object->next;
    freeObject(object);
    object = next;
  }
}

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{
  // if newSize is 0, free the memory block
  if (newSize == 0)
  {
    free(pointer);
    return NULL;
  }

  // else use the std library function to resize the memory block
  void *result = realloc(pointer, newSize);
  if (result == NULL)
    exit(1);
  return result;
}
