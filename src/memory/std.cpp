#include "std.hpp"
#include "utils.hpp"

extern "C" void* memset(void* dest, int ch, usize size) {
	u8* ptr = as<u8*>(dest);
	for (usize i = 0; i < size; ++i, ++ptr) {
		*ptr = as<u8>(ch);
	}
	return dest;
}

extern "C" void* memcpy(void* dest, const void* src, usize size) {
	u8* ptr = as<u8*>(dest);
	const u8* src_ptr = as<const u8*>(src);
	for (usize i = 0; i < size; ++i, ++ptr, ++src_ptr) {
		*ptr = *src_ptr;
	}
	return dest;
}