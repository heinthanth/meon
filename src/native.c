#include <string.h>

#include "native.h"

static Value getUnixEpoch(int argCount, Value *args)
{
    return NUMBER_VAL(time(NULL));
}

static Value clockNative(int argCount, Value *args)
{
    return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

static void defineNative(VM *vm, const char *name, NativeFn function)
{
    push(OBJ_VAL(cpString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm->globals, AS_STRING(vm->stack[0]), vm->stack[1]);
    pop();
    pop();
}

void loadNativeFunction(VM *vm)
{
    defineNative(vm, "time", getUnixEpoch);
    defineNative(vm, "clock", getUnixEpoch);
}