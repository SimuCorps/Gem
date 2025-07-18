//> Chunks of Bytecode memory-h
#ifndef gem_memory_h
#define gem_memory_h

#include "common.h"
//> Strings memory-include-object
#include "object.h"
//< Strings memory-include-object

//> Strings allocate
#define ALLOCATE(type, count) \
    (type*)reallocate(NULL, 0, sizeof(type) * (count))
//> free

#define FREE(type, pointer) reallocate(pointer, sizeof(type), 0)
//< free

//< Strings allocate
#define GROW_CAPACITY(capacity) \
    ((capacity) < 8 ? 8 : (capacity) * 2)
//> grow-array

#define GROW_ARRAY(type, pointer, oldCount, newCount) \
    (type*)reallocate(pointer, sizeof(type) * (oldCount), \
        sizeof(type) * (newCount))
//> free-array

#define FREE_ARRAY(type, pointer, oldCount) \
    reallocate(pointer, sizeof(type) * (oldCount), 0)
//< free-array

void* reallocate(void* pointer, size_t oldSize, size_t newSize);
//< grow-array
//> Strings free-objects-h
void freeObjects();
//< Strings free-objects-h

#endif
