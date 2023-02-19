#pragma once
#include "types.hpp"

constexpr inline usize pow2(u8 pow) {
	return 1ULL << pow;
}

inline usize log2(usize value) {
	auto count = __builtin_clzll(value - 1);
	return 8 * sizeof(usize) - count;
}

static u64 murmur64(u64 key) {
	key ^= key >> 33;
	key *= 0xff51afd7ed558ccdULL;
	key ^= key >> 33;
	key *= 0xc4ceb9fe1a85ec53ULL;
	key ^= key >> 33;
	return key;
}