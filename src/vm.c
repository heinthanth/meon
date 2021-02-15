#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "mem.h"
#include "vm.h"
#include "compiler.h"

VM vm;

static void resetStack()
{
    vm.stackSize = 0;
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getLine(vm.chunk, instruction);
    fprintf(stderr, "[line %d] in script\n", line);

    resetStack();
}

void initVM()
{
    vm.stack = NULL;
    vm.stackMaxSize = 0;
    resetStack();
    vm.objects = NULL;
}

void freeVM()
{
    freeObjects();
}

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

static Value peek(int distance)
{
    return vm.stack[distance];
}

static bool isFalse(Value value)
{
    return IS_BOOL(value) && !AS_BOOL(value);
}

static void concatenate()
{
    ObjectString *b = AS_STRING(pop());
    ObjectString *a = AS_STRING(pop());

    int length = a->length + b->length;
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjectString *result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static InterpretResult run()
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])

#define BINARY_OP(t, op)                                \
    do                                                  \
    {                                                   \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) \
        {                                               \
            runtimeError("Operands must be numbers.");  \
            return INTERPRET_RUNTIME_ERROR;             \
        }                                               \
        double b = AS_NUMBER(pop());                    \
        double a = AS_NUMBER(pop());                    \
        push(t(a op b));                                \
    } while (false)

#ifdef DEBUG_TRACE_EXECUTION
    printf("\n== %s ==\n", "execution trace");
#endif
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
        case OP_TRUE:
            push(BOOL_VAL(true));
            break;
        case OP_FALSE:
            push(BOOL_VAL(false));
            break;
        case OP_EQUAL:
        {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:
            BINARY_OP(BOOL_VAL, >);
            break;
        case OP_LESS:
            BINARY_OP(BOOL_VAL, <);
            break;
        case OP_GREATER_EQUAL:
            BINARY_OP(BOOL_VAL, >=);
            break;
        case OP_LESS_EQUAL:
            BINARY_OP(BOOL_VAL, <=);
            break;
        case OP_ADD:
            BINARY_OP(NUMBER_VAL, +);
            break;
        case OP_CONCAT:
        {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1)))
            {
                concatenate();
            }
            else
            {
                runtimeError("Operands must be strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT:
            BINARY_OP(NUMBER_VAL, -);
            break;
        case OP_MULTIPLY:
            BINARY_OP(NUMBER_VAL, *);
            break;
        case OP_DIVIDE:
            BINARY_OP(NUMBER_VAL, /);
            break;
        case OP_MODULO:
        {
            do
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL((int)a % (int)b));
            } while (false);
            break;
        }
        case OP_EXPONENT:
        {
            do
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(pow((int)a, (int)b)));
            } while (false);
            break;
        }
        case OP_NOT:
            push(BOOL_VAL(isFalse(pop())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0)))
            {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_RETURN:
        {
            printValue(pop());
            printf("\n\n");
            return INTERPRET_OK;
        }
        }
    }
#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult interpret(const char *source, const char *filename)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, filename, &chunk))
    {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run();
    freeChunk(&chunk);
    return result;
}