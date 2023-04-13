#pragma once
#include "types.h"

static inline u8 in1(u16 port) {
	u8 value;
	__asm__ volatile("in %0, %1" : "=a"(value) : "Nd"(port));
	return value;
}

static inline u16 in2(u16 port) {
	u16 value;
	__asm__ volatile("in %0, %1" : "=a"(value) : "Nd"(port));
	return value;
}

static inline u32 in4(u16 port) {
	u32 value;
	__asm__ volatile("in %0, %1" : "=a"(value) : "Nd"(port));
	return value;
}

static inline void out1(u16 port, u8 value) {
	__asm__ volatile("out %0, %1" : : "Nd"(port), "a"(value));
}

static inline void out2(u16 port, u16 value) {
	__asm__ volatile("out %0, %1" : : "Nd"(port), "a"(value));
}

static inline void out4(u16 port, u32 value) {
	__asm__ volatile("out %0, %1" : : "Nd"(port), "a"(value));
}

static inline void io_wait() {
	out1(0x80, 0);
}