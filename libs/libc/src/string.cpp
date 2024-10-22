#include "string.h"
#include <stdint.h>

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
	auto* dest_ptr = static_cast<unsigned char*>(dest);
	auto* src_ptr = static_cast<const unsigned char*>(src);

	if (reinterpret_cast<uintptr_t>(dest_ptr) % 8 != reinterpret_cast<uintptr_t>(src_ptr) % 8) {
		goto slow;
	}
	else if (size < 16) {
		goto fast_8;
	}

	while (reinterpret_cast<uintptr_t>(dest_ptr) % 8 ||
		   reinterpret_cast<uintptr_t>(src_ptr) % 8) {
		*dest_ptr++ = *src_ptr++;
		--size;
	}

	for (; size >= 64; size -= 64) {
		auto value0 = *reinterpret_cast<const uint64_t*>(src_ptr);
		auto value1 = *reinterpret_cast<const uint64_t*>(src_ptr + 8);
		auto value2 = *reinterpret_cast<const uint64_t*>(src_ptr + 16);
		auto value3 = *reinterpret_cast<const uint64_t*>(src_ptr + 24);
		auto value4 = *reinterpret_cast<const uint64_t*>(src_ptr + 32);
		auto value5 = *reinterpret_cast<const uint64_t*>(src_ptr + 40);
		auto value6 = *reinterpret_cast<const uint64_t*>(src_ptr + 48);
		auto value7 = *reinterpret_cast<const uint64_t*>(src_ptr + 56);
		asm volatile(
			"movnti %0, %1;"
			"movnti %2, %3;"
			"movnti %4, %5;"
			"movnti %6, %7;"

			"movnti %8, %9;"
			"movnti %10, %11;"
			"movnti %12, %13;"
			"movnti %14, %15" : :
			"r"(value0), "m"(*dest_ptr),
			"r"(value1), "m"(*(dest_ptr + 8)),
			"r"(value2), "m"(*(dest_ptr + 16)),
			"r"(value3), "m"(*(dest_ptr + 24)),

			"r"(value4), "m"(*(dest_ptr + 32)),
			"r"(value5), "m"(*(dest_ptr + 40)),
			"r"(value6), "m"(*(dest_ptr + 48)),
			"r"(value7), "m"(*(dest_ptr + 56)));
		src_ptr += 64;
		dest_ptr += 64;
	}

	for (; size >= 32; size -= 32) {
		auto value0 = *reinterpret_cast<const uint64_t*>(src_ptr);
		auto value1 = *reinterpret_cast<const uint64_t*>(src_ptr + 8);
		auto value2 = *reinterpret_cast<const uint64_t*>(src_ptr + 16);
		auto value3 = *reinterpret_cast<const uint64_t*>(src_ptr + 24);
		asm volatile(
			"movnti %0, %1;"
			"movnti %2, %3;"
			"movnti %4, %5;"
			"movnti %6, %7;" : :
			"r"(value0), "m"(*dest_ptr),
			"r"(value1), "m"(*(dest_ptr + 8)),
			"r"(value2), "m"(*(dest_ptr + 16)),
			"r"(value3), "m"(*(dest_ptr + 24)));
		src_ptr += 32;
		dest_ptr += 32;
	}

fast_8:
	for (; size >= 8; size -= 8) {
		auto value0 = *reinterpret_cast<const uint64_t*>(src_ptr);
		asm volatile(
			"movnti %0, %1;": :
			"r"(value0), "m"(*dest_ptr));
		src_ptr += 8;
		dest_ptr += 8;
	}

slow:
	for (; size; --size) {
		*dest_ptr++ = *src_ptr++;
	}

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
