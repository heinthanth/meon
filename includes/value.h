#ifndef meon_value_h
#define meon_value_h

#include "common.h"

typedef double Value;

typedef struct
{
    int size;
    int maxSize;
    Value *values;
} ValueArr;

void initValueArr(ValueArr *arr);
void writeValueArr(ValueArr *arr, Value value);
void freeValueArr(ValueArr *arr);
void printValue(Value value);

#endif