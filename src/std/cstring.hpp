#pragma once
#include "cstddef.hpp"

extern "C" {
	void* memcpy(void* __restrict dest, const void* __restrict src, size_t size);
	void* memset(void* __restrict dest, int ch, size_t size);
	void* memmove(void* __restrict dest, const void* __restrict src, size_t size);
	int memcmp(const void* __restrict a, const void* __restrict b, size_t size);

	size_t strlen(const char* str);
}

#define memcpy __builtin_memcpy
#define memset __builtin_memset
