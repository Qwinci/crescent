#include "arch.hpp"
#include "arch/x86/cpu.hpp"
#include "arch/x86/lapic.hpp"

void arch_sched_after_switch_from(Task* old_task, Task* this_task) {
	auto local = get_cpu_local();

	local->tss.rsp0_low = as<u32>(this_task->kernel_rsp);
	local->tss.rsp0_high = as<u32>(this_task->kernel_rsp >> 32);
}

extern X86CpuLocal* cpu_locals;

void arch_sched_load_balance_hlt(u8 cpu) {
	Lapic::send_ipi(cpu_locals[cpu].id, Lapic::Msg::LoadBalance);
}