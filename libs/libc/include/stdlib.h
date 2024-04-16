#ifndef _STDLIB_H
#define _STDLIB_H

#include "bits/utils.h"
#include <stddef.h>

__begin

void* malloc(size_t __size);
void* realloc(void* __old, size_t __new_size);
void free(void* __ptr);

__end

#endif
