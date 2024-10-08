#pragma once
#include "types.hpp"

static inline void arch_hlt() {
	asm volatile("wfi");
}

static inline bool arch_enable_irqs(bool enable) {
	u64 old;
	asm volatile("mrs %0, daif" : "=r"(old));

	// bit0 == fiq, bit1 == irq, bit2 == SError, bit3 == Debug
	if (enable) {
		asm volatile("msr daifclr, #0b10");
	}
	else {
		asm volatile("msr daifset, #0b10");
	}

	return !(old & 1 << 7);
}

static inline void mem_barrier() {
	asm volatile("dmb ish");
}

static inline void write_barrier() {
	asm volatile("dmb ishst" : : : "memory");
}

static inline void read_barrier() {
	asm volatile("dmb ishld");
}
