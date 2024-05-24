#pragma once
#include "types.hpp"

struct Checksum {
	inline void add(u16 value) {
		sum += value;
	}

	inline void add(const void* data, u32 size) {
		if (size % 2) {
			--size;
			add(static_cast<const u8*>(data)[size]);
		}

		for (u32 i = 0; i < size / 2; ++i) {
			sum += static_cast<const u16*>(data)[i];
		}
	}

	[[nodiscard]] inline u16 get() const {
		u32 res = (sum & 0xFFFF) + (sum >> 16);
		res = (res & 0xFFFF) + (res >> 16);
		return ~res;
	}

private:
	u32 sum = 0;
};
