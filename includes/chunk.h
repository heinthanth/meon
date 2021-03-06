#ifndef meon_chunk_h
#define meon_chunk_h

#include "common.h"
#include "value.h"

typedef enum
{
    OP_CONSTANT,
    OP_TRUE,
    OP_FALSE,
    OP_NULL,
    OP_POP,
    OP_GET_LOCAL,
    OP_GET_GLOBAL,
    OP_DEFINE_VAR_TYPE,
    OP_DEFINE_GLOBAL,
    //OP_DEFINE_LOCAL,
    OP_SET_LOCAL,
    OP_SET_GLOBAL,
    OP_GET_UPVALUE,
    OP_SET_UPVALUE,
    OP_CLOSE_UPVALUE,
    OP_EQUAL,
    OP_GREATER,
    OP_GREATER_EQUAL,
    OP_LESS,
    OP_LESS_EQUAL,
    OP_ADD,
    OP_CONCAT,
    OP_SUBTRACT,
    OP_MULTIPLY,
    OP_DIVIDE,
    OP_MODULO,
    OP_EXPONENT,
    OP_NOT,
    OP_NEGATE,
    OP_OUTPUT,
    OP_JUMP_IF_FALSE,
    OP_JUMP,
    OP_LOOP,
    OP_CALL,
    OP_CLOSURE,
    OP_RETURN
} OpCode;

typedef struct
{
    int offset;
    int line;
} LineStart;

typedef struct
{
    int size;
    int maxSize;
    uint8_t *code;
    ValueArr constants;

    int lineSize;
    int lineMaxSize;
    LineStart *lines;
} Chunk;

void initChunk(Chunk *chunk);
void writeChunk(Chunk *chunk, uint8_t b, int line);
void freeChunk(Chunk *chunk);

int addConstant(Chunk *chunk, Value value);
int getLine(Chunk *chunk, int instruction);

#endif