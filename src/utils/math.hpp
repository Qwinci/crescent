#pragma once
#include "types.hpp"

constexpr inline usize pow2(u8 pow) {
	return 1ULL << pow;
}

inline usize log2(usize value) {
	auto count = __builtin_clzll(value - 1);
	return 8 * sizeof(usize) - count;
}

static inline u64 murmur64(u64 key) {
	key ^= key >> 33;
	key *= 0xff51afd7ed558ccdULL;
	key ^= key >> 33;
	key *= 0xc4ceb9fe1a85ec53ULL;
	key ^= key >> 33;
	return key;
}

static inline u16 bswap16(u16 value) {
	return __builtin_bswap16(value);
}

static inline u32 bswap32(u32 value) {
	return __builtin_bswap32(value);
}

static inline u32 bswap64(u64 value) {
	return __builtin_bswap64(value);
}

template<typename T>
constexpr T max(T value1, T value2) {
	return value1 > value2 ? value1 : value2;
}

template<typename T>
constexpr T min(T value1, T value2) {
	return value1 < value2 ? value1 : value2;
}