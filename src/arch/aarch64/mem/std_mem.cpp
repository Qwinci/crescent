#include "cstring.hpp"
#include "types.hpp"

void* memcpy(void* __restrict dest, const void* __restrict src, size_t size) {
	auto* dest_ptr = static_cast<unsigned char*>(dest);
	auto* src_ptr = static_cast<const unsigned char*>(src);
	for (; size; --size) {
		*dest_ptr++ = *src_ptr++;
	}
	return dest;
}

void* memset(void* __restrict dest, int ch, size_t size) {
	if (!size) {
		return dest;
	}

	u64 c = static_cast<unsigned char>(ch);
	auto* ptr = static_cast<unsigned char*>(dest);

	for (; reinterpret_cast<usize>(ptr) & 7;) {
		*ptr++ = c;
		if (--size == 0) {
			return dest;
		}
	}

	c |= c << 8;
	c |= c << 16;
	c |= c << 32;

	for (; size >= 8; size -= 8) {
		*reinterpret_cast<u64*>(ptr) = c;
		ptr += 8;
	}

	for (; size; --size) {
		*ptr++ = c;
	}

	return dest;
}
