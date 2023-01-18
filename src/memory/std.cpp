#include "std.hpp"
#include "utils.hpp"

extern "C" void* memset(void* dest, int ch, usize size) {
	u8* ptr = as<u8*>(dest);
	for (usize i = 0; i < size; ++i, ++ptr) {
		*ptr = as<u8>(ch);
	}
	return dest;
}