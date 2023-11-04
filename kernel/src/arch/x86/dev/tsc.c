#include "tsc.h"
#include "arch/misc.h"
#include "arch/x86/cpu.h"

usize tsc_get_cycles() {
	u32 low, high;
	__asm__("rdtsc" : "=a"(low), "=d"(high));
	return (usize) low | (usize) high << 32;
}

void tsc_delay(usize us) {
	usize start = tsc_get_cycles();
	usize end = start + (x86_get_cur_cpu()->tsc_freq / 1000 / 1000) * us;
	while (tsc_get_cycles() < end) {
		arch_spinloop_hint();
	}
}