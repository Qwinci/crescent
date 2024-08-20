#pragma once
#include "types.hpp"
#include "config.hpp"
#include "cstring.hpp"

struct CpuSet {
	inline void set(u32 num) {
		assert(num < CONFIG_MAX_CPUS);
		inner[num / 32] |= 1U << (num % 32);
	}

	inline void clear(u32 num) {
		assert(num < CONFIG_MAX_CPUS);
		inner[num / 32] &= ~(1U << (num % 32));
	}

	inline bool test(u32 num) {
		return inner[num / 32] & 1U << (num % 32);
	}

	inline void clear_all() {
		memset(inner, 0, sizeof(inner));
	}

	inline void set_all() {
		memset(inner, 0xFF, sizeof(inner));
	}

	u32 inner[(CONFIG_MAX_CPUS + 31) & ~31];
};
