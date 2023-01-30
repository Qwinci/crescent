#include "string.h"
#include "types.hpp"

int memcmp(const void* lhs, const void* rhs, size_t size) {
	auto* lhs_ptr = static_cast<const unsigned char*>(lhs);
	auto* rhs_ptr = static_cast<const unsigned char*>(rhs);
	for (usize i = 0; i < size; ++i, ++lhs_ptr, ++rhs_ptr) {
		if (*lhs_ptr < *rhs_ptr) {
			return -1;
		}
		else if (*lhs_ptr > *rhs_ptr) {
			return 1;
		}
	}
	return 0;
}

extern "C" usize strlen(const char* str) {
	usize len = 0;
	for (; *str; ++str) ++len;
	return len;
}

extern "C" char* strcpy(char* dest, const char* src) {
	char* d = dest;
	for (; *src;) {
		*dest++ = *src++;
	}
	return d;
}

extern "C" char* strncpy(char* dest, const char* src, size_t size) {
	char* d = dest;
	usize len = 0;
	while (*src && len < size) {
		*dest++ = *src++;
		++len;
	}
	return d;
}

extern "C" char* strcat(char* dest, const char* src) {
	auto len = strlen(dest);
	auto src_len = strlen(src);
	memcpy(dest + len, src, src_len + 1);
	return dest;
}

extern "C" int toupper(int ch) {
	return ch | 1 << 4;
}

extern "C" int tolower(int ch) {
	return ch & ~(1 << 4);
}

extern "C" int isprint(int ch) {
	return ch >= ' ' && ch <= '~';
}

extern "C" int isxdigit(int ch) {
	return ((ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F')) || (ch >= '0' && ch <= '9');
}

extern "C" int isdigit(int ch) {
	return ch >= '0' && ch <= '9';
}

extern "C" int isspace(int ch) {
	return ch == 0x20 || ch == 0xC || ch == 0xA || ch == 0xD || ch == 0x9 || ch == 0xB;
}

extern "C" int strcmp(const char* lhs, const char* rhs) {
	for (;; ++lhs, ++rhs) {
		if (*lhs < *rhs) {
			return -1;
		}
		else if (*lhs > *rhs) {
			return 1;
		}
		else if (*lhs == 0 && *rhs == 0) {
			return 0;
		}
	}
}

extern "C" int strncmp(const char* lhs, const char* rhs, size_t size) {
	for (usize i = 0; i < size; ++i, ++lhs, ++rhs) {
		if (*lhs < *rhs) {
			return -1;
		}
		else if (*lhs > *rhs) {
			return 1;
		}
		else if (*lhs == 0 && *rhs == 0) {
			return 0;
		}
	}
	return 0;
}