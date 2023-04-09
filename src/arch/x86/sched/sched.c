#include "arch/cpu.h"
#include "arch/map.h"
#include "arch/misc.h"
#include "arch/x86/cpu.h"
#include "sched/sched_internals.h"
#include "x86_task.h"

X86Task* x86_switch_task(X86Task* old_task, X86Task* new_task);

void arch_sched_switch_from(Task*, Task* self) {
	X86Task* x86_self = container_of(self, X86Task, common);
	X86Cpu* cpu = x86_get_cur_cpu();
	cpu->tss.rsp0_low = x86_self->kernel_rsp;
	cpu->tss.rsp0_high = x86_self->kernel_rsp >> 32;

	if (!x86_self->user) {
		leave_critical((void*) true);
	}
}

void arch_switch_task(Task* self, Task* new_task) {
	X86Task* x86_self = container_of(self, X86Task, common);
	X86Task* x86_new = container_of(new_task, X86Task, common);

	if (x86_self->common.map != x86_new->common.map) {
		arch_use_map(x86_new->common.map);
	}
	x86_set_cpu_local(x86_new);

	X86Task* prev = x86_switch_task(x86_self, x86_new);

	x86_set_cpu_local(x86_self);
	if (prev->common.map != x86_self->common.map) {
		arch_use_map(x86_self->common.map);
	}

	sched_switch_from(&prev->common, &x86_self->common);
}