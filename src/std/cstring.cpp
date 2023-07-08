#include "cstring.hpp"
#include "types.hpp"

extern "C" {
void* memset(void* dest, int ch, size_t count) {
	auto c = static_cast<unsigned char>(ch);
	if (count < 5) {
		if (count == 0) {
			return dest;
		}
		auto ptr = static_cast<unsigned char*>(dest);
		ptr[0] = c;
		ptr[count - 1] = c;
		if (count <= 2) {
			return dest;
		}
		ptr[1] = c;
		ptr[2] = c;
		return dest;
	}
	else if (count <= 16) {
		u64 value8 = 0x0101010101010101ull * c;
		if (count >= 8) {
			auto start = static_cast<unsigned char*>(dest);
			auto end = start + count - 8;
			*reinterpret_cast<u64*>(start) = value8;
			*reinterpret_cast<u64*>(end) = value8;
			return dest;
		}

		u32 value4 = static_cast<u32>(value8);
		auto start = static_cast<unsigned char*>(dest);
		auto end = start + count - 4;
		*reinterpret_cast<u32*>(start) = value4;
		*reinterpret_cast<u32*>(end) = value4;
		return dest;
	}
	else if (count <= 32) {
		struct Value16 {
			unsigned char arr[16];
		};
		Value16 value16 {
			c, c, c, c, c, c, c, c,
			c, c, c, c, c, c, c, c};
		auto start = static_cast<Value16*>(dest);
		auto end = reinterpret_cast<Value16*>(reinterpret_cast<usize>(dest) + count - 16);
		*start = value16;
		*end = value16;
		return dest;
	}

	struct Value32 {
		unsigned char arr[32];
	};
	Value32 value32 {
		c, c, c, c, c, c, c, c,
		c, c, c, c, c, c, c, c,
		c, c, c, c, c, c, c, c,
		c, c, c, c, c, c, c, c
	};
	auto dest_ptr = reinterpret_cast<usize>(dest);
	void* end = reinterpret_cast<void*>(dest_ptr + count);
	void* last_32 = reinterpret_cast<void*>(reinterpret_cast<usize>(end) - 32);

	if (count <= 160) {
		auto ptr = static_cast<unsigned char*>(dest);
		do {
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
		} while (ptr < last_32);

		*reinterpret_cast<Value32*>(last_32) = value32;
		return dest;
	}
	else {
		*static_cast<Value32*>(dest) = value32;

		auto* aligned = reinterpret_cast<unsigned char*>(dest_ptr + 32 - (dest_ptr & (32 - 1)));

		auto ptr = aligned;
		while (ptr + (32 * 5) < end) {
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
		}

		if (ptr < end) {
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
		}
		if (ptr < end) {
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
		}
		if (ptr < end) {
			*reinterpret_cast<Value32*>(ptr) = value32;
			ptr += 32;
		}
		if (ptr < end) {
			*reinterpret_cast<Value32*>(ptr) = value32;
		}

		*reinterpret_cast<Value32*>(last_32) = value32;
		return dest;
	}
}

void* memcpy(void* __restrict dest, const void* __restrict src, size_t count) {
	auto d = static_cast<unsigned char*>(dest);
	auto s = static_cast<const unsigned char*>(src);

	if (count < 5) {
		if (count == 0) {
			return dest;
		}
		d[0] = s[0];
		d[count - 1] = s[count - 1];
		if (count <= 2) {
			return dest;
		}
		d[1] = s[1];
		d[2] = s[2];
		return dest;
	}
	else if (count <= 16) {
		if (count >= 8) {
			auto s_end8 = s + count - 8;
			auto d_end8 = d + count - 8;
			*reinterpret_cast<u64*>(d) = *reinterpret_cast<const u64*>(s);
			*reinterpret_cast<u64*>(d_end8) = *reinterpret_cast<const u64*>(s_end8);
			return dest;
		}

		auto s_end4 = s + count - 4;
		auto d_end4 = d + count - 4;
		*reinterpret_cast<u32*>(d) = *reinterpret_cast<const u32*>(s);
		*reinterpret_cast<u32*>(d_end4) = *reinterpret_cast<const u32*>(s_end4);
		return dest;
	}
	else if (count <= 32) {
		struct Value16 {
			unsigned char arr[16];
		};
		auto s_end16 = s + count - 16;
		auto d_end16 = d + count - 16;
		*reinterpret_cast<Value16*>(d) = *reinterpret_cast<const Value16*>(s);
		*reinterpret_cast<Value16*>(d_end16) = *reinterpret_cast<const Value16*>(s_end16);
		return dest;
	}
	else {
		auto s_end32 = s + count - 32;
		auto d_end32 = d + count - 32;

		struct Value32 {
			unsigned char arr[32];
		};

		do {
			*reinterpret_cast<Value32*>(d) = *reinterpret_cast<const Value32*>(s);
			d += 32;
			s += 32;
		} while (d < d_end32);

		*reinterpret_cast<Value32*>(d_end32) = *reinterpret_cast<const Value32*>(s_end32);
		return dest;
	}
}
}
