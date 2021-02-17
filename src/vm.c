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
#include "ansi-color.h"

VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, YEL "\nRUNTIME_ERROR: " RESET RED);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs(RESET "\n\n", stderr);

    size_t instruction = vm.ip - vm.chunk->code - 1;
    int line = getLine(vm.chunk, instruction);
    fprintf(stderr, YEL "%4d |" RESET " %s\n\n", line, "Somewhere in this Line :P");

    resetStack();
}

void initVM()
{
    resetStack();
    vm.objects = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);
}

void freeVM()
{
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value)
{
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop()
{
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance)
{
    return vm.stackTop[-1 - distance];
}

static bool isFalse(Value value)
{
    return IS_BOOL(value) && !AS_BOOL(value);
}

static void concatenate()
{
    char *b = value2string(pop());
    char *a = value2string(pop());

    int length = (int)(strlen(a) + strlen(b));
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a, strlen(a));
    memcpy(chars + strlen(a), b, strlen(b));

    chars[length] = '\0';
    ObjectString *result = takeString(chars, length);
    push(OBJ_VAL(result));
}

static InterpretResult run(int debugLevel)
{
#define READ_BYTE() (*vm.ip++)
#define READ_CONSTANT() (vm.chunk->constants.values[READ_BYTE()])
#define READ_SHORT() (vm.ip += 2, (uint16_t)((vm.ip[-2] << 8) | vm.ip[-1]))
#define READ_STRING() AS_STRING(READ_CONSTANT())

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

    //#ifdef DEBUG_TRACE_EXECUTION
    if (debugLevel > 1)
        printf("== %s ==\n", "execution trace");
    //#endif
    for (;;)
    {
        //#ifdef DEBUG_TRACE_EXECUTION
        if (debugLevel > 1)
        {
            printf("          ");
            for (Value* slot = vm.stack; slot < vm.stackTop; slot++)
            {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(vm.chunk, (int)(vm.ip - vm.chunk->code));
        }
        //#endif

        uint8_t instruction = READ_BYTE();
        switch (instruction)
        {
        case OP_CONSTANT:
        {
            Value constant = READ_CONSTANT();
            push(constant);
            break;
        }
        case OP_DEFINE_VAR_TYPE:
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
        case OP_POP:
            pop();
            break;
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            push(vm.stack[slot]);
            break;
        }
        case OP_GET_GLOBAL:
        {
            ObjectString *name = READ_STRING();
            Value value;
            if (!tableGet(&vm.globals, name, &value))
            {
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_LOCAL:
        {
            Value t = pop();
            if (
                (t.as.number == 24 && !IS_STRING(peek(0))) ||
                (t.as.number == 25 && !IS_NUMBER(peek(0))) ||
                (t.as.number == 26 && !IS_BOOL(peek(0))))
            {
                runtimeError("Cannot assign value to variable with different type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_DEFINE_GLOBAL:
        {
            ObjectString *name = READ_STRING();
            Value t = pop();
            Value value = pop();
            if (
                (t.as.number == 24 && !IS_STRING(value)) ||
                (t.as.number == 25 && !IS_NUMBER(value)) ||
                (t.as.number == 26 && !IS_BOOL(value)))
            {
                runtimeError("Cannot assign value to variable with different type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            tableSet(&vm.globals, name, value);
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            Value old = vm.stack[slot];
            if (old.t != peek(0).t)
            {
                runtimeError("Cannot assign value to variable with different type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            vm.stack[slot] = peek(0);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjectString *name = READ_STRING();
            TableItem *old = findTableItem(vm.globals.items, vm.globals.maxSize, name);

            if (old->k == NULL)
            {
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            if (old->v.t != peek(0).t)
            {
                runtimeError("Cannot assign value to variable with different type.");
                return INTERPRET_RUNTIME_ERROR;
            }
            tableSet(&vm.globals, name, peek(0));
            break;
        }
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
            concatenate();
            break;
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
        case OP_OUTPUT:
        {
            printValue(pop());
            printf("\n");
            break;
        }
        case OP_JUMP_IF_FALSE:
        {
            uint16_t offset = READ_SHORT();
            if (isFalse(peek(0)))
                vm.ip += offset;
            break;
        }
        case OP_JUMP:
        {
            uint16_t offset = READ_SHORT();
            vm.ip += offset;
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_SHORT();
            vm.ip -= offset;
            break;
        }
        case OP_RETURN:
        {
            // #ifdef DEBUG_TRACE_EXECUTION
            if (debugLevel > 1)
                printf("\n");
            //#endif
            return INTERPRET_OK;
        }
        }
    }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char *source, const char *filename, int debugLevel)
{
    Chunk chunk;
    initChunk(&chunk);

    if (!compile(source, filename, &chunk, debugLevel))
    {
        freeChunk(&chunk);
        return INTERPRET_COMPILE_ERROR;
    }

    vm.chunk = &chunk;
    vm.ip = vm.chunk->code;

    InterpretResult result = run(debugLevel);

    freeChunk(&chunk);
    return result;
}