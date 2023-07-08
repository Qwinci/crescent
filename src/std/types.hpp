#pragma once
#include <stddef.h>
#include <stdint.h>

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using isize = intptr_t;
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using usize = uintptr_t;

#if defined(__INT64_TYPE__) && defined(__UINT64_TYPE__)
using i64 = int64_t;
using u64 = uint64_t;
#endif
