#pragma once
#include "types.h"

static inline u8 in1(u16 port) {
	u8 value;
	__asm__ volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline u16 in2(u16 port) {
	u16 value;
	__asm__ volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline u32 in4(u16 port) {
	u32 value;
	__asm__ volatile("ind %1, %0" : "=a"(value) : "Nd"(port));
	return value;
}

static inline void out1(u16 port, u8 value) {
	__asm__ volatile("outb %1, %0" : : "Nd"(port), "a"(value));
}

static inline void out2(u16 port, u16 value) {
	__asm__ volatile("outw %1, %0" : : "Nd"(port), "a"(value));
}

static inline void out4(u16 port, u32 value) {
	__asm__ volatile("outd %1, %0" : : "Nd"(port), "a"(value));
}