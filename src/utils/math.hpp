#pragma once
#include "types.hpp"

constexpr inline usize pow2(u8 pow) {
	return 1ULL << pow;
}

inline usize log2(usize value) {
	auto count = __builtin_clzll(value - 1);
	return 8 * sizeof(usize) - count;
}