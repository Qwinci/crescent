#include "string.h"

void* memset(void* dest, int ch, size_t count) {
	void* dest_copy = dest;
	__asm__("rep stosb" : "+D"(dest_copy), "+c"(count) : "a"(ch) : "flags", "memory");
	return dest;
}

void* memcpy(void* dest, const void* src, size_t count) {
	void* dest_copy = dest;
	__asm__("rep movsb" : "+D"(dest_copy), "+S"(src), "+c"(count) : : "flags", "memory");
	return dest;
}