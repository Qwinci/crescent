#include "arch/interrupts.h"
#include "arch/misc.h"
#include "arch/x86/dev/lapic.h"
#include "cpu.h"
#include "types.h"
#include "arch/map.h"

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

void arch_invalidate_mapping(Process* process) {
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	Cpu* this_cpu = arch_get_cur_task()->cpu;

	spinlock_lock(&process->used_cpus_lock);
	for (usize i = 0; i < CONFIG_MAX_CPUS; ++i) {
		if (process->used_cpus[i / 32] & 1U << (i % 32)) {
			if (i == this_cpu->id) {
				// todo pcid
				if (this_cpu->cur_map == process->map) {
					arch_use_map(this_cpu->cur_map);
				}
			}
			else {
				X86Cpu* cpu = container_of(arch_get_cpu(i), X86Cpu, common);
				lapic_invalidate_mapping(cpu->apic_id, process->map);
			}
		}
	}
	spinlock_unlock(&process->used_cpus_lock);
	arch_ipl_set(old);
}