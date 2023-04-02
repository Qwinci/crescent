#include "arch/misc.h"

void arch_hlt() {
	__asm__ volatile("hlt");
}

void arch_spinloop_hint() {
	__builtin_ia32_pause();
}