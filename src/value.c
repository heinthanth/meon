#include <stdio.h>
#include <string.h>

#include "object.h"
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
    case VALUE_NULL:
        printf("null");
        break;
    case VALUE_BOOLEAN:
        printf(AS_BOOL(value) ? "true" : "false");
        break;
    case VALUE_NUMBER:
    {
        printf("%g", AS_NUMBER(value));
        break;
    }
    case VALUE_OBJECT:
        printObject(value);
        break;
    }
}

char *value2string(Value value)
{
    if (IS_BOOL(value))
    {
        char *str = AS_BOOL(value) ? "true" : "false";
        char *boolString = malloc(sizeof(char) * (strlen(str) + 1));
        snprintf(boolString, strlen(str) + 1, "%s", str);
        return boolString;
    }
    else if (IS_NUMBER(value))
    {
        double number = AS_NUMBER(value);
        int numberStringLength = snprintf(NULL, 0, "%.15g", number) + 1;
        char *numberString = malloc(sizeof(char) * numberStringLength);
        snprintf(numberString, numberStringLength, "%.15g", number);
        return numberString;
    }
    else if (IS_OBJ(value))
    {
        return object2string(value);
    }

    char *unknown = malloc(sizeof(char) * 8);
    snprintf(unknown, 8, "%s", "unknown");
    return unknown;
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
    case VALUE_OBJECT:
    {
        return AS_OBJ(a) == AS_OBJ(b);
    default:
        return false; // Unreachable.
    }
    }
}