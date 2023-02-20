#pragma once
#include <stdint.h>
#include <stdatomic.h>

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

using atomic_u8 = atomic_uint_fast8_t;
using atomic_u16 = atomic_uint_fast16_t;
using atomic_u32 = atomic_uint_fast32_t;
using atomic_u64 = atomic_uint_fast64_t;
using atomic_usize = atomic_uintptr_t;
using atomic_isize = atomic_intptr_t;