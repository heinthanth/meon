#include <stdio.h>

#include "mem.h"
#include "value.h"

void initValueArr(ValueArr *arr)
{
    arr->size = 0;
    arr->maxSize = 0;
    arr->values = NULL;
}

void writeValueArr(ValueArr *arr, Value value)
{
    if (arr->maxSize < arr->size + 1)
    {
        int old = arr->maxSize;
        arr->maxSize = GROW_ARRAY_SIZE(old);
        arr->values = GROW_ARRAY(Value, arr->values, old, arr->maxSize);
    }
    arr->values[arr->size] = value;
    arr->size++;
}

void freeValueArr(ValueArr *arr)
{
    FREE_ARRAY(Value, arr->values, arr->maxSize);
    initValueArr(arr);
}

void printValue(Value value)
{
    switch (value.t)
    {
    case VALUE_BOOLEAN:
        printf(AS_BOOL(value) ? "true" : "false");
        break;
    case VALUE_NUMBER:
        printf("%g", AS_NUMBER(value));
        break;
    }
}

bool valuesEqual(Value a, Value b)
{
    if (a.t != b.t)
        return false;

    switch (a.t)
    {
    case VALUE_BOOLEAN:
        return AS_BOOL(a) == AS_BOOL(b);
    case VALUE_NUMBER:
        return AS_NUMBER(a) == AS_NUMBER(b);
    default:
        return false; // Unreachable.
    }
}