#include "chunk.h"
#include "mem.h"

void initChunk(Chunk *chunk)
{
    chunk->size = 0;
    chunk->maxSize = 0;
    chunk->code = NULL;
    chunk->lines = NULL;
    initValueArr(&chunk->constants);
}

void writeChunk(Chunk *chunk, uint8_t b, int line)
{
    if (chunk->maxSize < chunk->size + 1)
    {
        int old = chunk->size;
        chunk->maxSize = GROW_ARRAY_SIZE(old);
        chunk->code = GROW_ARRAY(uint8_t, chunk->code, old, chunk->maxSize);
        chunk->lines = GROW_ARRAY(int, chunk->lines, old, chunk->maxSize);
    }
    chunk->code[chunk->size] = b;
    chunk->lines[chunk->size] = line;
    chunk->size++;
}

void freeChunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->maxSize);
    FREE_ARRAY(int, chunk->lines, chunk->maxSize);
    freeValueArr(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk *chunk, Value value)
{
    writeValueArr(&chunk->constants, value);
    return chunk->constants.size - 1;
}