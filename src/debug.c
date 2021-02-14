#include <stdio.h>

#include "debug.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name)
{
    printf("\n== %s ==\n\n", name);
    for (int offset = 0; offset < chunk->size;)
    {
        offset = disassembleInstruction(chunk, offset);
    }
}

static int constantInstruction(const char *name, Chunk *chunk, int offset)
{
    uint8_t constant = chunk->code[offset + 1];
    printf("%-16s %4d '", name, constant);
    printValue(chunk->constants.values[constant]);
    printf("\n");
    return offset + 2;
}

static int simpleInstruction(const char *name, int offset)
{
    printf("%s\n", name);
    return offset + 1;
}

int disassembleInstruction(Chunk *chunk, int offset)
{
    printf("%04X ", offset);
    int line = getLine(chunk, offset);
    if (offset > 0 && line == getLine(chunk, offset - 1))
    {
        printf("   | ");
    }
    else
    {
        printf("%4d ", line);
    }

    uint8_t instruction = chunk->code[offset];
    switch (instruction)
    {
    case OP_CONSTANT:
        return constantInstruction("const", chunk, offset);
    case OP_ADD:
        return simpleInstruction("add", offset);
    case OP_SUBTRACT:
        return simpleInstruction("sub", offset);
    case OP_MULTIPLY:
        return simpleInstruction("mul", offset);
    case OP_DIVIDE:
        return simpleInstruction("div", offset);
    case OP_MODULO:
        return simpleInstruction("mod", offset);
    case OP_EXPONENT:
        return simpleInstruction("exp", offset);
    case OP_NEGATE:
        return simpleInstruction("neg", offset);
    case OP_RETURN:
        return simpleInstruction("ret", offset);
    default:
        printf("Unknown OpCode %d\n", instruction);
        return offset + 1;
    }
}