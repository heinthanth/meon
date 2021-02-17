#ifndef meon_object_h
#define meon_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->t)

#define IS_STRING(value) check_object_t(value, OBJECT_STRING)
#define IS_FUNCTION(value) check_object_t(value, OBJECT_FUNCTION)

#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJ(value))
#define AS_STRING(value) ((ObjectString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJ(value))->chars)

typedef enum
{
    OBJECT_STRING,
    OBJECT_FUNCTION
} object_t;

struct Object
{
    object_t t;
    struct Object *next;
};

typedef struct
{
    Object object;
    int argsCount;
    Chunk chunk;
    ObjectString *name;
} ObjectFunction;

struct ObjectString
{
    Object object;
    int length;
    char *chars;
    uint32_t hash;
};

ObjectFunction *newFunction();
ObjectString *takeString(char *chars, int length);
ObjectString *cpString(const char *chars, int length);

static inline bool check_object_t(Value value, object_t t)
{
    return IS_OBJ(value) && AS_OBJ(value)->t == t;
}

void printObject(Value value);
char *object2string(Value value);

#endif