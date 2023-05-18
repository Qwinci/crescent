#include "arch/interrupts.h"
#include "arch/misc.h"
#include "arch/x86/dev/lapic.h"
#include "cpu.h"
#include "types.h"

void arch_hlt() {
	__asm__ volatile("hlt");
}

void arch_spinloop_hint() {
	__builtin_ia32_pause();
}

void arch_hlt_cpu(usize index) {
	X86Cpu* cpu = container_of(arch_get_cpu(index), X86Cpu, common);

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	lapic_ipi(cpu->apic_id, LAPIC_MSG_HALT);
	arch_ipl_set(old);
}