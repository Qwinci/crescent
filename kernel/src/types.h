#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef uintptr_t usize;
typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef intptr_t isize;
typedef float f32;
typedef double f64;

typedef atomic_uint_fast8_t atomic_u8;
typedef atomic_uint_fast16_t atomic_u16;
typedef atomic_uint_fast32_t atomic_u32;
typedef atomic_uint_fast64_t atomic_u64;
typedef atomic_uintptr_t atomic_usize;
typedef atomic_int_fast8_t atomic_i8;
typedef atomic_int_fast16_t atomic_i16;
typedef atomic_int_fast32_t atomic_i32;
typedef atomic_int_fast64_t atomic_i64;
typedef atomic_intptr_t atomic_isize;
typedef _Atomic(float) atomic_f32;
typedef _Atomic(double) atomic_f64;

#define container_of(ptr, base, field) ((base*) ((usize) (ptr) - offsetof(base, field)))
#define offset(ptr, type, off) ((type) ((usize) (ptr) + (off)))