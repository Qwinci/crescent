#include "cpu.h"
#include "arch/x86/sched/x86_task.h"

void x86_set_cpu_local(X86Task* task) {
	x86_set_msr(MSR_GSBASE, (u64) task);
}

void x86_set_msr(Msr msr, u64 value) {
	__asm__ volatile("wrmsr" : : "a"((u32) value), "d"((u32)(value >> 32)), "c"(msr));
}

u64 x86_get_msr(Msr msr) {
	u32 low, high;
	__asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
	return (u64) low | (u64) high << 32;
}

Task* arch_get_cur_task() {
	X86Task* task;
	__asm__("mov %%gs:0, %0" : "=r"(task));
	return &task->common;
}

X86Cpu* x86_get_cur_cpu() {
	X86Task* task;
	__asm__("mov %%gs:0, %0" : "=r"(task));
	return container_of(task->common.cpu, X86Cpu, common);
}