#ifndef meon_vm_h
#define meon_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 256
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct
{
    ObjectClosure *closure;
    uint8_t *ip;
    Value *slots;
} CallFrame;

typedef struct
{
    CallFrame frames[FRAMES_MAX];
    int frameCount;
    Value stack[STACK_MAX];
    Value *stackTop;
    Table globals;
    Table strings;
    ObjectUpvalue *openUpvalues;

    size_t bytesAllocated;
    size_t nextGC;

    Object *objects;
    int grayCount;
    int grayCapacity;
    Object **grayStack;
} VM;

typedef enum
{
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char *source, const char *filename, int debugLevel);
void push(Value value);
Value pop();

#endif