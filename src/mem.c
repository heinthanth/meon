#include <stdlib.h>
#include "mem.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

void *reallocate(void *pointer, size_t oldSize, size_t newSize)
{
    vm.bytesAllocated += newSize - oldSize;
    if (newSize > oldSize)
    {
#ifdef DEBUG_STRESS_GC
        collectGarbage();
#endif
    }
    if (vm.bytesAllocated > vm.nextGC)
    {
        collectGarbage();
    }

    if (newSize == 0)
    {
        free(pointer);
        return NULL;
    }
    void *result = realloc(pointer, newSize);
    if (result == NULL)
        exit(1);
    return result;
}

void markObject(Object *object)
{
    if (object == NULL)
        return;
    if (object->isMarked)
        return;
#ifdef DEBUG_LOG_GC
    printf("%p mark ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    object->isMarked = true;
    if (vm.grayCapacity < vm.grayCount + 1)
    {
        vm.grayCapacity = GROW_ARRAY_SIZE(vm.grayCapacity);
        vm.grayStack = realloc(vm.grayStack,
                               sizeof(Object *) * vm.grayCapacity);

        if (vm.grayStack == NULL)
            exit(1);
    }

    vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value)
{
    if (!IS_OBJ(value))
        return;
    markObject(AS_OBJ(value));
}

static void markArray(ValueArr *array)
{
    for (int i = 0; i < array->maxSize; i++)
    {
        markValue(array->values[i]);
    }
}

static void blackenObject(Object *object)
{
#ifdef DEBUG_LOG_GC
    printf("%p blacken ", (void *)object);
    printValue(OBJ_VAL(object));
    printf("\n");
#endif
    switch (object->t)
    {
    case OBJECT_CLOSURE:
    {
        ObjectClosure *closure = (ObjectClosure *)object;
        markObject((Object *)closure->function);
        for (int i = 0; i < closure->upvalueCount; i++)
        {
            markObject((Object *)closure->upvalues[i]);
        }
        break;
    }
    case OBJECT_FUNCTION:
    {
        ObjectFunction *function = (ObjectFunction *)object;
        markObject((Object *)function->name);
        markArray(&function->chunk.constants);
        break;
    }
    case OBJECT_UPVALUE:
        markValue(((ObjectUpvalue *)object)->closed);
        break;
    case OBJECT_NATIVE:
    case OBJECT_STRING:
        break;
    }
}

static void freeObject(Object *object)
{
#ifdef DEBUG_LOG_GC
    printf("%p free type %d\n", (void *)object, object->t);
#endif
    switch (object->t)
    {
    case OBJECT_STRING:
    {
        ObjectString *string = (ObjectString *)object;
        FREE_ARRAY(char, string->chars, string->length + 1);
        FREE(ObjectString, object);
        break;
    }
    case OBJECT_FUNCTION:
    {
        ObjectFunction *function = (ObjectFunction *)object;
        freeChunk(&function->chunk);
        FREE(ObjectFunction, object);
        break;
    }
    case OBJECT_NATIVE:
        FREE(ObjectNative, object);
        break;
    case OBJECT_CLOSURE:
    {
        ObjectClosure *closure = (ObjectClosure *)object;
        FREE_ARRAY(ObjectUpvalue *, closure->upvalues, closure->upvalueCount);
        FREE(ObjectClosure, object);
        break;
    }
    case OBJECT_UPVALUE:
        FREE(ObjectUpvalue, object);
        break;
    }
}

static void markRoots()
{
    for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
    {
        markValue(*slot);
    }
    for (int i = 0; i < vm.frameCount; i++)
    {
        markObject((Object *)vm.frames[i].closure);
    }
    for (ObjectUpvalue *upvalue = vm.openUpvalues;
         upvalue != NULL;
         upvalue = upvalue->next)
    {
        markObject((Object *)upvalue);
    }

    markTable(&vm.globals);
    markCompilerRoots();
}

static void traceReferences()
{
    while (vm.grayCount > 0)
    {
        Object *object = vm.grayStack[--vm.grayCount];
        blackenObject(object);
    }
}

static void sweep()
{
    Object *previous = NULL;
    Object *object = vm.objects;
    while (object != NULL)
    {
        if (object->isMarked)
        {
            object->isMarked = false;
            previous = object;
            object = object->next;
        }
        else
        {
            Object *unreached = object;
            object = object->next;
            if (previous != NULL)
            {
                previous->next = object;
            }
            else
            {
                vm.objects = object;
            }

            freeObject(unreached);
        }
    }
}

void collectGarbage()
{
#ifdef DEBUG_LOG_GC
    printf("-- gc begin\n");
    size_t before = vm.bytesAllocated;
#endif

    markRoots();
    traceReferences();
    tableRemoveWhite(&vm.strings);
    sweep();

    vm.nextGC = vm.bytesAllocated * GC_HEAP_GROW_FACTOR;

#ifdef DEBUG_LOG_GC
    printf("-- gc end\n");
    printf("   collected %ld bytes (from %ld to %ld) next at %ld\n",
           before - vm.bytesAllocated, before, vm.bytesAllocated,
           vm.nextGC);
#endif
}

void freeObjects()
{
    Object *object = vm.objects;
    while (object != NULL)
    {
        Object *next = object->next;
        freeObject(object);
        object = next;
    }
    free(vm.grayStack);
}