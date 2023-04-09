#include "dev/timer.h"
#include "arch/x86/cpu.h"
#include "arch/x86/cpuid.h"
#include "hpet.h"
#include "stdio.h"
#include "tsc.h"

static bool hpet_initialized = false;

void udelay(usize us) {
	X86Cpu* cpu = x86_get_cur_cpu();
	if (cpu->tsc_freq) {
		tsc_delay(us);
	}
	else if (hpet_initialized) {
		hpet_delay(us);
	}
}

void mdelay(usize ms) {
	udelay(ms * 1000);
}

void arch_init_timers() {
	hpet_initialized = hpet_init();

	X86Cpu* cpu = x86_get_cur_cpu();
	Cpuid inv_tsc_cpuid = cpuid(0x80000007, 0);
	if (!(inv_tsc_cpuid.edx & 1 << 8)) {
		return;
	}

	if (!hpet_initialized) {
		panic("[kernel][timer]: tsc is invariant but no other timer is present\n");
	}

	usize start = tsc_get_cycles();
	udelay(10 * 1000);
	usize end = tsc_get_cycles();

	usize diff = end - start;
	cpu->tsc_freq = diff * 100;
	kprintf("[kernel][timer]: tsc frequency: %uhz\n", cpu->tsc_freq);
}

usize arch_get_ns_since_boot() {
	X86Cpu* cpu = x86_get_cur_cpu();
	if (cpu->tsc_freq) {
		usize cycles = tsc_get_cycles();
		return (cycles * 1000 * 1000 * 1000) / cpu->tsc_freq;
	}
	else if (cpu->lapic_timer.freq) {
		return lapic_timer_get_ns();
	}
	return 0;
}

void arch_create_timer(usize time, void (*fn)()) {

}