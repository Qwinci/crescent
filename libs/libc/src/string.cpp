#include "string.h"

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

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
