#ifndef meon_value_h
#define meon_value_h

#include "common.h"

typedef enum
{
    VALUE_BOOLEAN,
    VALUE_NUMBER
} value_t;

typedef struct
{
    value_t t;
    union
    {
        bool boolean;
        double number;
    } as;
} Value;

#define IS_BOOL(value) ((value).t == VALUE_BOOLEAN)
#define IS_NUMBER(value) ((value).t == VALUE_NUMBER)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value) ((Value){VALUE_BOOLEAN, {.boolean = value}})
#define NUMBER_VAL(value) ((Value){VALUE_NUMBER, {.number = value}})

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
bool valuesEqual(Value a, Value b);

#endif