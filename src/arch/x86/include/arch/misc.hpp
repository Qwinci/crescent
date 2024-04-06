#pragma once
#include "types.hpp"

static inline void arch_hlt() {
	asm volatile("hlt");
}

static inline bool arch_enable_irqs(bool enable) {
	u64 old;
	asm volatile("pushfq; pop %0" : "=rm"(old));
	if (enable) {
		asm volatile("sti");
	}
	else {
		asm volatile("cli");
	}
	return old & 1 << 9;
}
