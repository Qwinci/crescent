#include "string.h"
#include "utils/attribs.h"

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

int strcmp(const char* lhs, const char* rhs) {
	for (;; ++lhs, ++rhs) {
		int res = *lhs - *rhs;
		if (res != 0) {
			return res;
		}
		else if (!*rhs || !*lhs) {
			return 0;
		}
	}
}

int strncmp(const char* lhs, const char* rhs, size_t count) {
	for (; count; ++lhs, ++rhs, --count) {
		int res = *lhs - *rhs;
		if (res != 0 || !*rhs || !*lhs) {
			return res;
		}
	}
	return 0;
}

char* strcpy(char* restrict dest, const char* restrict src) {
	char* dest_ptr = dest;
	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return dest_ptr;
}

char* strncpy(char* restrict dest, const char* restrict src, size_t count) {
	char* dest_ptr = dest;
	for (; count > 1 && *src; --count) {
		*dest_ptr++ = *src++;
	}
	*dest_ptr = 0;
	return dest;
}

char* strcat(char* restrict dest, const char* restrict src) {
	char* dest_ptr = dest;
	size_t len = strlen(dest);
	dest += len;

	while (*src) {
		*dest++ = *src++;
	}
	*dest = 0;
	return dest_ptr;
}

int memcmp(const void* lhs, const void* rhs, size_t count) {
	const unsigned char* lhs_ptr = (const unsigned char*) lhs;
	const unsigned char* rhs_ptr = (const unsigned char*) rhs;
	for (; count; ++lhs_ptr, ++rhs_ptr, --count) {
		int res = *lhs_ptr - *rhs_ptr;
		if (res != 0) {
			return res;
		}
	}
	return 0;
}

WEAK void* memset(void* dest, int ch, size_t count) {
	unsigned char* dest_ptr = (unsigned char*) dest;
	for (; count; --count) {
		*dest_ptr++ = (unsigned char) ch;
	}
	return dest;
}

WEAK void* memcpy(void* dest, const void* restrict src, size_t count) {
	unsigned char* dest_ptr = (unsigned char*) dest;
	const unsigned char* src_ptr = (const unsigned char*) src;
	for (; count; --count) {
		*dest_ptr++ = *src_ptr++;
	}
	return dest;
}

void* memmove(void* dest, const void* src, size_t count) {
	if (dest < src) {
		return memcpy(dest, src, count);
	}
	unsigned char* dest_ptr = (unsigned char*) dest + count;
	const unsigned char* src_ptr = (unsigned char*) src + count;
	for (; count; --count) {
		*--dest_ptr = *--src_ptr;
	}
	return dest;
}