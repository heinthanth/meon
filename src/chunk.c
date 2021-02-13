#include "chunk.h"
#include "mem.h"

void initChunk(Chunk *chunk)
{
    chunk->size = 0;
    chunk->maxSize = 0;
    chunk->code = NULL;
    chunk->lineSize = 0;
    chunk->lineMaxSize = 0;
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
    }
    chunk->code[chunk->size] = b;
    chunk->size++;

    if (chunk->lineSize > 0 &&
        chunk->lines[chunk->lineSize - 1].line == line)
    {
        return;
    }

    if (chunk->lineMaxSize < chunk->lineSize + 1)
    {
        int oldCapacity = chunk->lineMaxSize;
        chunk->lineMaxSize = GROW_ARRAY_SIZE(oldCapacity);
        chunk->lines = GROW_ARRAY(LineStart, chunk->lines, oldCapacity, chunk->lineMaxSize);
    }

    LineStart *lineStart = &chunk->lines[chunk->lineSize++];
    lineStart->offset = chunk->size - 1;
    lineStart->line = line;
}

void freeChunk(Chunk *chunk)
{
    FREE_ARRAY(uint8_t, chunk->code, chunk->maxSize);
    FREE_ARRAY(LineStart, chunk->lines, chunk->lineMaxSize);
    freeValueArr(&chunk->constants);
    initChunk(chunk);
}

int addConstant(Chunk *chunk, Value value)
{
    writeValueArr(&chunk->constants, value);
    return chunk->constants.size - 1;
}

int getLine(Chunk *chunk, int instruction)
{
    int start = 0;
    int end = chunk->lineSize - 1;

    for (;;)
    {
        int mid = (start + end) / 2;
        LineStart *line = &chunk->lines[mid];
        if (instruction < line->offset)
        {
            end = mid - 1;
        }
        else if (mid == chunk->lineSize - 1 ||
                 instruction < chunk->lines[mid + 1].offset)
        {
            return line->line;
        }
        else
        {
            start = mid + 1;
        }
    }
}