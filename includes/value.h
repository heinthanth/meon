#ifndef meon_value_h
#define meon_value_h

#include <stdlib.h>

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef enum
{
    VALUE_BOOLEAN,
    VALUE_NUMBER,
    VALUE_OBJECT,
} value_t;

typedef struct
{
    value_t t;
    union
    {
        bool boolean;
        double number;
        Object *object;
    } as;
} Value;

#define IS_BOOL(value) ((value).t == VALUE_BOOLEAN)
#define IS_NUMBER(value) ((value).t == VALUE_NUMBER)
#define IS_OBJ(value) ((value).t == VALUE_OBJECT)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_OBJ(value) ((value).as.object)

#define BOOL_VAL(value) ((Value){VALUE_BOOLEAN, {.boolean = value}})
#define NUMBER_VAL(value) ((Value){VALUE_NUMBER, {.number = value}})
#define OBJ_VAL(obj)   ((Value){VALUE_OBJECT, {.object = (Object*)obj}})

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
char *value2string(Value value);
bool valuesEqual(Value a, Value b);

#endif