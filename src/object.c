#include <stdio.h>
#include <string.h>

#include "mem.h"
#include "object.h"
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

static ObjectString *allocateString(char *chars, int length)
{
    ObjectString *string = ALLOCATE_OBJ(ObjectString, OBJECT_STRING);
    string->length = length;
    string->chars = chars;
    return string;
}

ObjectString *cpString(const char *chars, int length)
{
    char *heapChars = ALLOCATE(char, length + 1);
    memcpy(heapChars, chars, length);
    heapChars[length] = '\0';

    return allocateString(heapChars, length);
}

void printObject(Value value)
{
    switch (OBJ_TYPE(value))
    {
    case OBJECT_STRING:
        printf("%s", AS_CSTRING(value));
        break;
    }
}

ObjectString *takeString(char *chars, int length)
{
    return allocateString(chars, length);
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
    }
    char *unknown = malloc(sizeof(char) * 9);
    snprintf(unknown, 8, "%s", "unknown");
    return unknown;
}