#include "arch.hpp"

void arch_spinloop_hint() {
	__builtin_ia32_pause();
}

void arch_hlt() {
	asm volatile("hlt");
}