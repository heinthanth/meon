#ifndef meon_compiler_h
#define meon_compiler_h

#include "vm.h"
#include "object.h"

ObjectFunction* compile(const char *source, const char *filename, int debugLeevl);

#endif