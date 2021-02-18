#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "object.h"
#include "table.h"
#include "value.h"
#include "vm.h"

#define ALLOCATE_OBJ(t, ot) \
    (t *)allocateObject(sizeof(t), ot);

static Object *allocateObject(size_t size, object_t t)
{
    Object *object = (Object *)reallocate(NULL, 0, size);
    object->t = t;
    object->isMarked = false;

    object->next = vm.objects;
    vm.objects = object;
#ifdef DEBUG_LOG_GC
    printf("%p allocate %ld for %d\n", (void *)object, size, t);
#endif

    return object;
}

ObjectFunction *newFunction()
{
    ObjectFunction *function = ALLOCATE_OBJ(ObjectFunction, OBJECT_FUNCTION);

    function->argsCount = 0;
    function->name = NULL;
    function->upvalueCount = 0;
    initChunk(&function->chunk);
    return function;
}

static ObjectString *allocateString(char *chars, int length, uint32_t hash)
{
    ObjectString *string = ALLOCATE_OBJ(ObjectString, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    push(OBJ_VAL(string));
    tableSet(&vm.strings, string, NULL_VAL);
    pop();
    return string;
}

static uint32_t hashString(const char *k, int length)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < length; i++)
    {
        hash ^= (uint8_t)k[i];
        hash *= 16777619;
    }
    return hash;
}

ObjectString *cpString(const char *chars, int length)
{
    uint32_t hash = hashString(chars, length);
    ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
        return interned;

    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length, hash);
}

ObjectUpvalue *newUpvalue(Value *slot)
{
    ObjectUpvalue *upvalue = ALLOCATE_OBJ(ObjectUpvalue, OBJECT_UPVALUE);
    upvalue->closed = NULL_VAL;
    upvalue->location = slot;
    upvalue->next = NULL;
    return upvalue;
}

static void printFunction(ObjectFunction *function)
{
    if (function->name == NULL)
    {
        printf("[ script ]");
        return;
    }
    printf("[ func %s ]", function->name->chars);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJECT_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    case OBJECT_FUNCTION:
        printFunction(AS_FUNCTION(value));
        break;
    case OBJECT_CLOSURE:
        printFunction(AS_CLOSURE(value)->function);
        break;
    case OBJECT_NATIVE:
        printf("[ native func ]");
        break;
    case OBJECT_UPVALUE:
        printf("upvalue");
        break;
    }
}

ObjectNative *newNative(NativeFn function)
{
    ObjectNative *native = ALLOCATE_OBJ(ObjectNative, OBJECT_NATIVE);
    native->function = function;
    return native;
}

ObjectClosure *newClosure(ObjectFunction *function)
{
    ObjectUpvalue **upvalues = ALLOCATE(ObjectUpvalue *, function->upvalueCount);
    for (int i = 0; i < function->upvalueCount; i++)
    {
        upvalues[i] = NULL;
    }

    ObjectClosure *closure = ALLOCATE_OBJ(ObjectClosure, OBJECT_CLOSURE);
    closure->function = function;
    closure->upvalues = upvalues;
    closure->upvalueCount = function->upvalueCount;
    return closure;
}

ObjectString *takeString(char *chars, int length)
{
    uint32_t hash = hashString(chars, length);
    ObjectString *interned = tableFindString(&vm.strings, chars, length, hash);
    if (interned != NULL)
    {
        FREE_ARRAY(char, chars, length + 1);
        return interned;
    }
    return allocateString(chars, length, hash);
}

char *object2string(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJECT_STRING:
    {
        ObjectString *stringObj = AS_STRING(value);
        char *string = malloc(sizeof(char) * stringObj->length + 3);
        snprintf(string, stringObj->length + 3, "%s", stringObj->chars);
        return string;
    }
    case OBJECT_FUNCTION:
    {
        ObjectFunction *function = AS_FUNCTION(value);
        if (function->name == NULL)
        {
            return "[ script ]";
        }
        char *string = malloc(sizeof(char) * function->name->length + 12);
        snprintf(string, function->name->length + 12, "[ func %s ]", function->name->chars);
        return string;
    }
    case OBJECT_NATIVE:
    {
        char *native = malloc(sizeof(char) * 12);
        snprintf(native, 11, "%s", "[ script ]");
        return native;
    }
    case OBJECT_CLOSURE:
    {
        ObjectFunction *function = AS_CLOSURE(value)->function;
        if (function->name == NULL)
        {
            return "[ closure ]";
        }
        char *string = malloc(sizeof(char) * function->name->length + 12);
        snprintf(string, function->name->length + 12, "[ func %s ]", function->name->chars);
        return string;
        break;
    }
    case OBJECT_UPVALUE:
    {
        char *up = malloc(sizeof(char) * 13);
        snprintf(up, 12, "%s", "[ upvalue ]");
        return up;
    }
    default:
    {
        char *unknown = malloc(sizeof(char) * 9);
        snprintf(unknown, 8, "%s", "unknown");
        return unknown;
    }
    }
}