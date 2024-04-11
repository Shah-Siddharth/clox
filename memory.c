#include <stdlib.h>

#include "memory.h"

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
  // if newSize is 0, free the memory block
  if (newSize == 0) {
    free(pointer);
    return NULL;
  }

  // else use the std library function to resize the memory block
  void* result = realloc(pointer, newSize);
  if (result == NULL) exit(1);
  return result;
}
