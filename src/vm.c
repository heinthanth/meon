#include <stdio.h>

#include "common.h"
#include "debug.h"
#include "vm.h"
#include "mem.h"

VM vm;

static void resetStack()
{
    vm.stackSize = 0;
}

void initVM()
{
    vm.stack = NULL;
    vm.stackMaxSize = 0;
    resetStack();
}

void freeVM() {}

void push(Value value)
{
    if (vm.stackMaxSize < vm.stackSize + 1)
    {
        int old = vm.stackMaxSize;
        vm.stackMaxSize = GROW_ARRAY_SIZE(old);
        vm.stack = GROW_ARRAY(Value, vm.stack, old, vm.stackMaxSize);
    }
    vm.stack[vm.stackSize] = value;
    vm.stackSize++;
}

Value pop()
{
    vm.stackSize--;
    return vm.stack[vm.stackSize];
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(op)     \
    do                    \
    {                     \
        double b = pop(); \
        double a = pop(); \
        push(a op b);     \
    } while (false)

    for (;;)
    {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (int i = 0; i < vm.stackSize; i++)
        {
            printf("[ ");
            printValue(vm.stack[i]);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
#endif

        uint8_t instruction;
        switch (instruction = READ_BYTE())
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_ADD:
            BINARY_OP(+);
            break;
        case OP_SUBTRACT:
            BINARY_OP(-);
            break;
        case OP_MULTIPLY:
            BINARY_OP(*);
            break;
        case OP_DIVIDE:
            BINARY_OP(/);
            break;
        case OP_NEGATE:
            push(-pop());
            break;
        case OP_RETURN:
        {
            printValue(pop());
            printf("\n");
            return INTERPRET_OK;
        }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(Chunk *chunk)
{
    vm.chunk = chunk;
    vm.ip = vm.chunk->code;
    return run();
}