#include "std.hpp"

extern "C" void* memset(void* dest, int ch, usize size) {
	auto dest_copy = dest;
	asm("rep stosb" : "+D"(dest_copy), "+c"(size) : "a"(ch) : "flags", "memory");
	return dest;
}

extern "C" void* memcpy(void* dest, const void* src, usize size) {
	auto dest_copy = dest;
	asm("rep movsb" : "+D"(dest_copy), "+S"(src), "+c"(size) : : "flags", "memory");
	return dest;
}