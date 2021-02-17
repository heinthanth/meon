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

    object->next = vm.objects;
    vm.objects = object;
    return object;
}

ObjectFunction *newFunction()
{
    ObjectFunction *function = ALLOCATE_OBJ(ObjectFunction, OBJECT_FUNCTION);

    function->argsCount = 0;
    function->name = NULL;
    initChunk(&function->chunk);
    return function;
}

static ObjectString *allocateString(char *chars, int length, uint32_t hash)
{
    ObjectString *string = ALLOCATE_OBJ(ObjectString, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    string->hash = hash;
    tableSet(&vm.strings, string, NULL_VAL);
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
    }
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
    default:
    {
        char *unknown = malloc(sizeof(char) * 9);
        snprintf(unknown, 8, "%s", "unknown");
        return unknown;
    }
    }
}