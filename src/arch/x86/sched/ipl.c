#include "arch/interrupts.h"

Ipl ipl_get() {
	u64 ipl;
	__asm__("mov %0, cr8" : "=r"(ipl));
	return (Ipl) ipl;
}

static void ipl_set(Ipl ipl) {
	__asm__ volatile("mov cr8, %0" : : "r"(ipl));
}

Ipl ipl_raise(Ipl new_ipl) {
	Ipl old_ipl = ipl_get();
	if (old_ipl < new_ipl) {
		ipl_set(new_ipl);
	}
	return old_ipl;
}

void ipl_lower(Ipl old_ipl) {
	if (ipl_get() > old_ipl) {
		ipl_set(old_ipl);
	}
}