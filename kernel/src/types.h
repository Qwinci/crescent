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

typedef _Atomic(uint8_t) atomic_u8;
typedef _Atomic(uint16_t) atomic_u16;
typedef _Atomic(uint32_t) atomic_u32;
typedef _Atomic(uint64_t) atomic_u64;
typedef atomic_uintptr_t atomic_usize;
typedef _Atomic(int8_t) atomic_i8;
typedef _Atomic(int16_t) atomic_i16;
typedef _Atomic(int32_t) atomic_i32;
typedef _Atomic(int64_t) atomic_i64;
typedef atomic_intptr_t atomic_isize;
typedef _Atomic(float) atomic_f32;
typedef _Atomic(double) atomic_f64;

#define container_of(ptr, base, field) ((base*) ((usize) (ptr) - offsetof(base, field)))
#define offset(ptr, type, off) ((type) ((usize) (ptr) + (off)))