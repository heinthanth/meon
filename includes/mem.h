#ifndef meon_mem_h
#define meon_mem_h

#include "common.h"

#define GROW_ARRAY_SIZE(maxSize) ((maxSize) < 8 ? 8 : (maxSize)*2)
#define GROW_ARRAY(t, pointer, oldSize, newSize) \
    (t *)reallocate(pointer, sizeof(t) * (oldSize), sizeof(t) * (newSize))
#define FREE_ARRAY(t, pointer, oldSize) \
    reallocate(pointer, sizeof(t) * (oldSize), 0)

void *reallocate(void *pointer, size_t oldSize, size_t newSize);

#endif