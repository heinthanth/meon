#include <stdio.h>

#include "debug.h"
#include "object.h"
#include "value.h"

void disassembleChunk(Chunk *chunk, const char *name)
{
    printf("\n== %s ==\n\n", name);
    for (int offset = 0; offset < chunk->size;)
    {
        offset = disassembleInstruction(chunk, offset);
    }
    printf("\n");
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

static int bInstruction(const char *name, Chunk *chunk,
                        int offset)
{
    uint8_t slot = chunk->code[offset + 1];
    printf("%-16s %4d\n", name, slot);
    return offset + 2;
}

static int jumpInstruction(const char *name, int sign, Chunk *chunk, int offset)
{
    uint16_t jump = (uint16_t)(chunk->code[offset + 1] << 8);
    jump |= chunk->code[offset + 2];
    printf("%-16s %04d -> %04d\n", name, offset, offset + 3 + sign * jump);
    return offset + 3;
}

int disassembleInstruction(Chunk *chunk, int offset)
{
    printf("%04d ", offset);
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
    case OP_TRUE:
        return simpleInstruction("true", offset);
    case OP_FALSE:
        return simpleInstruction("false", offset);
    case OP_NULL:
        return simpleInstruction("nul", offset);
    case OP_POP:
        return simpleInstruction("pop", offset);
    case OP_GET_LOCAL:
        return bInstruction("lget", chunk, offset);
    case OP_SET_LOCAL:
        return bInstruction("lset", chunk, offset);
    case OP_GET_GLOBAL:
        return constantInstruction("gget", chunk, offset);
    case OP_DEFINE_GLOBAL:
        return constantInstruction("gdef", chunk, offset);
    // case OP_DEFINE_LOCAL:
    //     return simpleInstruction("ldef", offset);
    case OP_DEFINE_VAR_TYPE:
        return constantInstruction("dvt", chunk, offset);
    case OP_SET_GLOBAL:
        return constantInstruction("gset", chunk, offset);
    case OP_GET_UPVALUE:
        return bInstruction("uvget", chunk, offset);
    case OP_CLOSE_UPVALUE:
      return simpleInstruction("uvclose", offset);
    case OP_SET_UPVALUE:
        return bInstruction("uvset", chunk, offset);
    case OP_EQUAL:
        return simpleInstruction("eq", offset);
    case OP_GREATER:
        return simpleInstruction("gt", offset);
    case OP_LESS:
        return simpleInstruction("lt", offset);
    case OP_GREATER_EQUAL:
        return simpleInstruction("ge", offset);
    case OP_LESS_EQUAL:
        return simpleInstruction("le", offset);
    case OP_ADD:
        return simpleInstruction("add", offset);
    case OP_CONCAT:
        return simpleInstruction("concat", offset);
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
    case OP_NOT:
        return simpleInstruction("not", offset);
    case OP_NEGATE:
        return simpleInstruction("neg", offset);
    case OP_OUTPUT:
        return simpleInstruction("output", offset);
    case OP_JUMP:
        return jumpInstruction("jmp", 1, chunk, offset);
    case OP_JUMP_IF_FALSE:
        return jumpInstruction("jif", 1, chunk, offset);
    case OP_LOOP:
        return jumpInstruction("loop", -1, chunk, offset);
    case OP_CALL:
        return bInstruction("call", chunk, offset);
    case OP_CLOSURE:
    {
        offset++;
        uint8_t constant = chunk->code[offset++];
        printf("%-16s %4d ", "OP_CLOSURE", constant);
        printValue(chunk->constants.values[constant]);
        printf("\n");

        ObjectFunction *function = AS_FUNCTION(chunk->constants.values[constant]);
        for (int j = 0; j < function->upvalueCount; j++)
        {
            int isLocal = chunk->code[offset++];
            int index = chunk->code[offset++];
            printf("%04d      |                     %s %d\n",
                   offset - 2, isLocal ? "local" : "upvalue", index);
        }

        return offset;
    }
    case OP_RETURN:
        return simpleInstruction("ret", offset);
    default:
        printf("Unknown OpCode %d\n", instruction);
        return offset + 1;
    }
}