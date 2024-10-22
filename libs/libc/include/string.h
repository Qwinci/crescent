#ifndef _STRING_H
#define _STRING_H

#include "bits/utils.h"
#include <stddef.h>

__begin

size_t strlen(const char* __str);
void* memset(void* __dest, int __ch, size_t __size);
void* memcpy(void* __dest, const void* __src, size_t __size);

__end

#endif
