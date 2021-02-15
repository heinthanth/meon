#ifndef meon_compiler_h
#define meon_compiler_h

#include "vm.h"
#include "object.h"

bool compile(const char *source, const char *filename, Chunk *chunk);

#endif