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

static bool x86_disable_interrupts_with_prev() {
	u16 flags;
	__asm__ volatile("pushfw; pop %0; cli" : "=r"(flags));
	return flags & 1 << 9;
}

static void x86_enable_interrupts() {
	__asm__ volatile("sti");
}

void* enter_critical() {
	return (void*) x86_disable_interrupts_with_prev();
}

void leave_critical(void* flags) {
	if (flags) {
		x86_enable_interrupts();
	}
}

void arch_hlt_cpu(usize index) {
	X86Cpu* cpu = container_of(arch_get_cpu(index), X86Cpu, common);

	void* flags = enter_critical();
	lapic_ipi(cpu->apic_id, LAPIC_MSG_HALT);
	leave_critical(flags);
}