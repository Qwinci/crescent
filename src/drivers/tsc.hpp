#pragma once
#include "types.hpp"

static inline u64 read_timestamp() {
	u32 low, high;
	asm volatile("rdtsc" : "=a"(low), "=d"(high));
	return as<u64>(low) | as<u64>(high) << 32;
}