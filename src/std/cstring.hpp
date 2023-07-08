#pragma once
#include <stddef.h>

extern "C" {
void* memset(void* dest, int ch, size_t count);
void* memcpy(void* __restrict dest, const void* __restrict src, size_t count);
}