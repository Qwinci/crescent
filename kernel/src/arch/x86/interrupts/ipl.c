#include "arch/interrupts.h"
#include "ipl.h"

X86Ipl X86_IPL_MAP[] = {
	[IPL_NORMAL] = INT_PRIO_2,
	[IPL_DEV] = INT_PRIO_3,
	[IPL_TIMER] = INT_PRIO_13,
	[IPL_INTER_CPU] = INT_PRIO_14,
	[IPL_CRITICAL] = INT_PRIO_15
};

static Ipl X86_IPL_X86_MAP[] = {
	[INT_PRIO_2] = IPL_NORMAL,
	[INT_PRIO_3] = IPL_DEV,
	[INT_PRIO_13] = IPL_TIMER,
	[INT_PRIO_14] = IPL_INTER_CPU,
	[INT_PRIO_15] = IPL_CRITICAL
};

static X86Ipl x86_ipl_get() {
	u64 ipl;
	__asm__("mov %%cr8, %0" : "=r"(ipl));
	return (X86Ipl) ipl;
}

static void x86_ipl_set(X86Ipl ipl) {
	__asm__ volatile("mov %0, %%cr8" : : "r"(ipl));
}

Ipl arch_ipl_set(Ipl ipl) {
	Ipl old = X86_IPL_X86_MAP[x86_ipl_get()];
	x86_ipl_set(X86_IPL_MAP[ipl]);
	return old;
}
