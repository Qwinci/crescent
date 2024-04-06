#include "cstring.hpp"

void* memmove(void* __restrict dest, const void* __restrict src, size_t size) {
	if (dest < src) {
		return memcpy(dest, src, size);
	}
	else {
		auto* dest_ptr = static_cast<unsigned char*>(dest) + size;
		auto* src_ptr = static_cast<const unsigned char*>(src) + size;
		for (; size; --size) {
			*--dest_ptr = *--src_ptr;
		}
		return dest;
	}
}

[[gnu::weak]] void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
	auto* dest_ptr = static_cast<unsigned char*>(dest);
	auto* src_ptr = static_cast<const unsigned char*>(src);
	for (; size; --size) {
		*dest_ptr++ = *src_ptr++;
	}
	return dest;
}

[[gnu::weak]] void* memset(void* __restrict dest, int ch, size_t size) {
	auto* dest_ptr = static_cast<unsigned char*>(dest);
	auto c = static_cast<unsigned char>(ch);
	for (; size; --size) {
		*dest_ptr++ = c;
	}
	return dest;
}

int memcmp(const void* __restrict a, const void* __restrict b, size_t size) {
	auto* a_ptr = static_cast<const unsigned char*>(a);
	auto* b_ptr = static_cast<const unsigned char*>(b);
	for (; size; --size) {
		auto res = *a_ptr++ - *b_ptr++;
		if (res != 0) {
			return res;
		}
	}
	return 0;
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}
