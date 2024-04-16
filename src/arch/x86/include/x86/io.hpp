#pragma once

namespace x86 {
	static inline u8 in1(u16 port) {
		u8 value;
		asm volatile("inb %1, %0" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline u16 in2(u16 port) {
		u16 value;
		asm volatile("inw %1, %0" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline u32 in4(u16 port) {
		u32 value;
		asm volatile("inl %1, %0" : "=a"(value) : "Nd"(port));
		return value;
	}

	static inline void out1(u16 port, u8 value) {
		asm volatile("outb %1, %0" : : "Nd"(port), "a"(value));
	}

	static inline void out2(u16 port, u16 value) {
		asm volatile("outw %1, %0" : : "Nd"(port), "a"(value));
	}

	static inline void out4(u16 port, u32 value) {
		asm volatile("outl %1, %0" : : "Nd"(port), "a"(value));
	}
}
