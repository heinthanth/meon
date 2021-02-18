#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

#include "common.h"
#include "debug.h"
#include "object.h"
#include "mem.h"
#include "vm.h"
#include "compiler.h"
#include "ansi-color.h"
#include "native.h"

VM vm;

static void resetStack()
{
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    fprintf(stderr, YEL "\nRUNTIME_ERROR: " RESET RED);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs(RESET "\n\n", stderr);

    CallFrame *frame = &vm.frames[vm.frameCount - 1];
    ObjectFunction *function = frame->closure->function;

    size_t instruction = frame->ip - function->chunk.code - 1;
    int line = getLine(&function->chunk, instruction);
    fprintf(stderr, YEL "%4d |>" RESET " %s\n", line, "Somewhere in this Line :P");

    fprintf(stderr, YEL "\nSTACK: \n\n" RESET RED);
    for (int i = vm.frameCount - 1; i >= 0; i--)
    {
        CallFrame *frame = &vm.frames[i];
        ObjectFunction *function = frame->closure->function;
        // -1 because the IP is sitting on the next instruction to be
        // executed.
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, YEL "%4d |>" RESET " from " YEL, i + 1);
        if (function->name == NULL)
        {
            fprintf(stderr, "script");
        }
        else
        {
            fprintf(stderr, "%s", function->name->chars);
        }
        fprintf(stderr, RESET " at " YEL "line %d\n" RESET, getLine(&function->chunk, instruction));
    }
    printf("\n");
    resetStack();
}

void initVM()
{
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024;
    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;
    initTable(&vm.globals);
    initTable(&vm.strings);

    loadNativeFunction(&vm);
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

static bool call(ObjectClosure *closure, int argCount)
{
    if (argCount != closure->function->argsCount)
    {
        runtimeError("Expected %d arguments but got %d.", closure->function->argsCount, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX)
    {
        runtimeError("Oops! stack OVERFLOW.");
        return false;
    }

    CallFrame *frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;

    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount)
{
    if (IS_OBJ(callee))
    {
        switch (OBJ_TYPE(callee))
        {
        case OBJECT_NATIVE:
        {
            NativeFn native = AS_NATIVE(callee);
            Value result = native(argCount, vm.stackTop - argCount);
            vm.stackTop -= argCount + 1;
            push(result);
            return true;
        }
        case OBJECT_CLOSURE:
            return call(AS_CLOSURE(callee), argCount);
        default:
            // Non-callable object type.
            break;
        }
    }

    runtimeError("Non-functions and non-classes can't be invoked.");
    return false;
}

static ObjectUpvalue *captureUpvalue(Value *local)
{
    ObjectUpvalue *prevUpvalue = NULL;
    ObjectUpvalue *upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local)
    {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local)
    {
        return upvalue;
    }
    ObjectUpvalue *createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL)
    {
        vm.openUpvalues = createdUpvalue;
    }
    else
    {
        prevUpvalue->next = createdUpvalue;
    }
    return createdUpvalue;
}

static void closeUpvalues(Value *last)
{
    while (vm.openUpvalues != NULL &&
           vm.openUpvalues->location >= last)
    {
        ObjectUpvalue *upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static bool isFalse(Value value)
{
    return IS_BOOL(value) && !AS_BOOL(value);
}

static void concatenate()
{
    char *b = value2string(peek(0));
    char *a = value2string(peek(1));

    int length = (int)(strlen(a) + strlen(b));
    char *chars = ALLOCATE(char, length + 1);
    memcpy(chars, a, strlen(a));
    memcpy(chars + strlen(a), b, strlen(b));

    chars[length] = '\0';
    ObjectString *result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static InterpretResult run(int debugLevel)
{
    CallFrame *frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
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
            for (Value *slot = vm.stack; slot < vm.stackTop; slot++)
            {
                printf("[ ");
                printValue(*slot);
                printf(" ]");
            }
            printf("\n");
            disassembleInstruction(&frame->closure->function->chunk, (int)(frame->ip - frame->closure->function->chunk.code));
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
        case OP_NULL:
            push(NULL_VAL);
            break;
        case OP_POP:
            pop();
            break;
        case OP_GET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            push(frame->slots[slot]);
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
        // case OP_DEFINE_LOCAL:
        // {
        //     Value t = pop();
        //     if (
        //         (t.as.number == 24 && !IS_STRING(peek(0))) ||
        //         (t.as.number == 25 && !IS_NUMBER(peek(0))) ||
        //         (t.as.number == 26 && !IS_BOOL(peek(0))))
        //     {
        //         runtimeError("Cannot assign value to variable with different type.");
        //         return INTERPRET_RUNTIME_ERROR;
        //     }
        //     break;
        // }
        case OP_DEFINE_GLOBAL:
        {
            ObjectString *name = READ_STRING();
            //Value t = pop();
            Value value = pop();
            // if (
            //     (t.as.number == 24 && !IS_STRING(value)) ||
            //     (t.as.number == 25 && !IS_NUMBER(value)) ||
            //     (t.as.number == 26 && !IS_BOOL(value)))
            // {
            //     runtimeError("Cannot assign value to variable with different type.");
            //     return INTERPRET_RUNTIME_ERROR;
            // }
            tableSet(&vm.globals, name, value);
            break;
        }
        case OP_SET_LOCAL:
        {
            uint8_t slot = READ_BYTE();
            // Value old = frame->slots[slot];
            // if (old.t != peek(0).t)
            // {
            //     runtimeError("Cannot assign value to variable with different type.");
            //     return INTERPRET_RUNTIME_ERROR;
            // }
            frame->slots[slot] = peek(0);
            break;
        }
        case OP_SET_GLOBAL:
        {
            ObjectString *name = READ_STRING();
            if (tableSet(&vm.globals, name, peek(0)))
            {
                tableDelete(&vm.globals, name);
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_GET_UPVALUE:
        {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE:
        {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
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
        {
            do
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                if (b == 0)
                {
                    runtimeError("Divisor must not be 'zero'.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                push(NUMBER_VAL(a / b));
            } while (false);
            break;
        }
        case OP_MODULO:
        {
            do
            {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                if (b == 0)
                {
                    runtimeError("Divisor must not be 'zero'.");
                    return INTERPRET_RUNTIME_ERROR;
                }
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
                frame->ip += offset;
            break;
        }
        case OP_JUMP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_LOOP:
        {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL:
        {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount))
            {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_CLOSURE:
        {
            ObjectFunction *function = AS_FUNCTION(READ_CONSTANT());
            ObjectClosure *closure = newClosure(function);
            push(OBJ_VAL(closure));
            for (int i = 0; i < closure->upvalueCount; i++)
            {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal)
                {
                    closure->upvalues[i] =
                        captureUpvalue(frame->slots + index);
                }
                else
                {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }
        case OP_CLOSE_UPVALUE:
        {
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        }
        case OP_RETURN:
        {
            // #ifdef DEBUG_TRACE_EXECUTION
            if (debugLevel > 1)
                printf("\n");
            //#endif
            Value result = pop();
            closeUpvalues(frame->slots);
            vm.frameCount--;
            if (vm.frameCount == 0)
            {
                pop();
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots;
            push(result);

            frame = &vm.frames[vm.frameCount - 1];
            break;
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
    ObjectFunction *function = compile(source, filename, debugLevel);
    if (function == NULL)
        return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjectClosure *closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callValue(OBJ_VAL(closure), 0);

    return run(debugLevel);
}