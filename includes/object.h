#ifndef meon_object_h
#define meon_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value) (AS_OBJ(value)->t)

#define IS_STRING(value) check_object_t(value, OBJECT_STRING)
#define IS_CLOSURE(value) check_object_t(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) check_object_t(value, OBJECT_FUNCTION)
#define IS_NATIVE(value) check_object_t(value, OBJECT_NATIVE)

#define AS_CLOSURE(value) ((ObjectClosure *)AS_OBJ(value))
#define AS_FUNCTION(value) ((ObjectFunction *)AS_OBJ(value))
#define AS_NATIVE(value) (((ObjectNative *)AS_OBJ(value))->function)
#define AS_STRING(value) ((ObjectString *)AS_OBJ(value))
#define AS_CSTRING(value) (((ObjectString *)AS_OBJ(value))->chars)

typedef enum
{
    OBJECT_STRING,
    OBJECT_FUNCTION,
    OBJECT_NATIVE,
    OBJECT_CLOSURE,
    OBJECT_UPVALUE,
} object_t;

struct Object
{
    object_t t;
    struct Object *next;
    bool isMarked;
};

typedef struct
{
    Object object;
    int argsCount;
    int upvalueCount;
    Chunk chunk;
    ObjectString *name;
} ObjectFunction;

typedef Value (*NativeFn)(int argCount, Value *args);

typedef struct
{
    Object object;
    NativeFn function;
} ObjectNative;

struct ObjectString
{
    Object object;
    int length;
    char *chars;
    uint32_t hash;
};

typedef struct ObjectUpvalue
{
    Object obj;
    Value *location;
     Value closed;
    struct ObjectUpvalue *next;
} ObjectUpvalue;

typedef struct
{
    Object obj;
    ObjectFunction *function;
    ObjectUpvalue **upvalues;
    int upvalueCount;
} ObjectClosure;

ObjectFunction *newFunction();
ObjectNative *newNative(NativeFn function);
ObjectClosure *newClosure(ObjectFunction *function);
ObjectUpvalue *newUpvalue(Value *slot);
ObjectString *takeString(char *chars, int length);
ObjectString *cpString(const char *chars, int length);

static inline bool check_object_t(Value value, object_t t)
{
    return IS_OBJ(value) && AS_OBJ(value)->t == t;
}

void printObject(Value value);
char *object2string(Value value);

#endif