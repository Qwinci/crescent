#include "string.h"

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

#ifdef __x86_64__

void* memset(void* __restrict dest, int ch, size_t size) {
	void* dest_copy = dest;
	asm volatile("rep stosb" : "+D"(dest_copy), "+c"(size) : "a"(ch) : "flags", "memory");
	return dest;
}

void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
	void* dest_copy = dest;
	asm volatile("rep movsb" : "+D"(dest_copy), "+S"(src), "+c"(size) : : "flags", "memory");
	return dest;
}

#else

void* memset(void* dest, int ch, size_t size) {
	auto* ptr = static_cast<unsigned char*>(dest);
	for (; size; --size) {
		*ptr++ = static_cast<unsigned char>(ch);
	}
	return dest;
}

void* memcpy(void* dest, const void* src, size_t size) {
	auto* dest_ptr = static_cast<unsigned char*>(dest);
	auto* src_ptr = static_cast<const unsigned char*>(src);
	for (; size; --size) {
		*dest_ptr++ = *src_ptr++;
	}
	return dest;
}

#endif
