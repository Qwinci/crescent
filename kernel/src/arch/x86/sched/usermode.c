#include "usermode.h"
#include "arch/x86/cpu.h"

extern void x86_syscall_entry();

#define USER_SEG_BASE 0x18ULL

void x86_init_usermode() {
	u64 efer = x86_get_msr(MSR_EFER);
	efer |= 1;
	x86_set_msr(MSR_EFER, efer);

	x86_set_msr(MSR_LSTAR, (usize) x86_syscall_entry);

	u64 star = 8ULL << 32 | USER_SEG_BASE << 48;
	x86_set_msr(MSR_STAR, star);

	x86_set_msr(MSR_SFMASK, x86_get_msr(MSR_SFMASK) | 1ULL << 9);
}