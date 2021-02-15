#ifndef meon_mem_h
#define meon_mem_h

#include "common.h"
#include "object.h"

#define GROW_ARRAY_SIZE(maxSize) ((maxSize) < 8 ? 8 : (maxSize)*2)
#define GROW_ARRAY(t, pointer, oldSize, newSize) \
    (t *)reallocate(pointer, sizeof(t) * (oldSize), sizeof(t) * (newSize))
#define FREE_ARRAY(t, pointer, oldSize) \
    reallocate(pointer, sizeof(t) * (oldSize), 0)
#define ALLOCATE(t, count) \
    (t *)reallocate(NULL, 0, sizeof(t) * (count))
#define FREE(t, pointer) reallocate(pointer, sizeof(t), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);
void freeObjects();

#endif