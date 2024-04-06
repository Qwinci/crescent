#pragma once
#include "types.hpp"

static inline void arch_hlt() {
	asm volatile("wfi");
}

static inline bool arch_enable_irqs(bool enable) {
	u64 old;
	asm volatile("mrs %0, DAIF" : "=r"(old));

	// bit0 == fiq, bit1 == irq, bit2 == SError, bit3 == Debug
	if (enable) {
		asm volatile("msr DAIFClr, #0b10");
	}
	else {
		asm volatile("msr DAIFSet, #0b10");
	}

	return !(old & 1 << 7);
}
