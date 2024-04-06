#pragma once
#include "cstdint.hpp"
#include "cstddef.hpp"

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using usize = uintptr_t;
using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;
using isize = intptr_t;

static constexpr usize operator ""_usize(unsigned long long value) {
	return value;
}

#define offset(ptr, type, offset) ((type) ((usize) (ptr) + (offset)))
