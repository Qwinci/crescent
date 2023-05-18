#include "arch/cpu.h"
#include "arch/misc.h"
#include "arch/x86/dev/lapic.h"
#include "arch/x86/interrupts/gdt.h"
#include "arch/x86/interrupts/idt.h"
#include "arch/x86/sched/usermode.h"
#include "arch/x86/sched/x86_task.h"
#include "assert.h"
#include "cpu.h"
#include "limine/limine.h"
#include "mem/allocator.h"
#include "mem/page.h"
#include "mem/vm.h"
#include "sched/sched_internals.h"
#include "stdio.h"
#include "string.h"
#include "utils/spinlock.h"

static volatile struct limine_smp_request SMP_REQUEST = {
	.id = LIMINE_SMP_REQUEST
};

static usize CPU_COUNT = 0;
static atomic_size_t CPUS_ONLINE = 1;
static Spinlock START_LOCK = {};
static X86Cpu* CPUS = NULL;
u8 X86_BSP_ID = 0;

[[noreturn]] static void x86_ap_entry(struct limine_smp_info* info);

[[noreturn]] static void x86_ap_entry_asm(struct limine_smp_info* info) {
	spinlock_lock(&START_LOCK);
	x86_load_idt();
	arch_use_map(KERNEL_MAP);

	__asm__ volatile("mov rsp, %0; xor rbp, rbp; call %P2" : : "r"(info->extra_argument), "D"(info), "i"(x86_ap_entry) : "rsp", "rbp");
	panic("x86_ap_entry_asm unreachable code\n");
}

static X86Task* create_this_task(Cpu* cpu) {
	X86Task* self = (X86Task*) kmalloc(sizeof(X86Task));
	assert(self);
	memset(self, 0, sizeof(X86Task));
	self->common.status = TASK_STATUS_RUNNING;
	self->common.map = KERNEL_MAP;
	memcpy(self->common.name, "kernel main", sizeof("kernel main"));
	self->common.level = SCHED_MAX_LEVEL - 1;
	self->common.cpu = cpu;
	self->common.pin_cpu = true;
	self->self = self;
	return self;
}

[[noreturn]] static void x86_ap_entry(struct limine_smp_info* info) {
	X86Cpu* cpu = &CPUS[CPU_COUNT++];

	cpu->apic_id = info->lapic_id;
	cpu->tss.iopb = sizeof(Tss);
	x86_load_gdt(&cpu->tss);

	X86Task* this_task = create_this_task(&cpu->common);
	cpu->common.current_task = &this_task->common;
	cpu->common.thread_count += 1;
	cpu->lapic_timer.period_us = US_IN_SEC;
	x86_set_cpu_local(this_task);

	__asm__ volatile("mov ax, 6 * 8; ltr ax" : : : "ax");

	lapic_init();
	lapic_timer_init();

	CPUS_ONLINE += 1;

	x86_init_usermode();
	sched_init(false);

	spinlock_unlock(&START_LOCK);
	lapic_timer_start(1);
	sched_block(TASK_STATUS_WAITING);
	panic("x86_ap_entry resumed\n");
}

void arch_init_smp() {
	assert(SMP_REQUEST.response);
	X86_BSP_ID = SMP_REQUEST.response->bsp_lapic_id;

	CPUS = kmalloc(SMP_REQUEST.response->cpu_count * sizeof(X86Cpu));
	assert(CPUS);
	memset(CPUS, 0, SMP_REQUEST.response->cpu_count * sizeof(X86Cpu));
	for (usize i = 0; i < SMP_REQUEST.response->cpu_count; ++i) {
		CPUS[i].self = &CPUS[i];
	}

	CPUS[0].apic_id = SMP_REQUEST.response->bsp_lapic_id;
	CPUS[0].tss.iopb = sizeof(Tss);
	X86Task* this_task = create_this_task(&CPUS[0].common);
	CPUS[0].common.current_task = &this_task->common;
	CPUS[0].common.thread_count += 1;
	CPUS[0].lapic_timer.period_us = US_IN_SEC;
	CPU_COUNT = 1;
	x86_load_gdt(&CPUS[0].tss);
	x86_set_cpu_local(this_task);
	__asm__ volatile("mov ax, 6 * 8; ltr ax" : : : "ax");

	arch_init_timers();
	lapic_first_init();
	lapic_init();
	lapic_timer_init();
	kprintf("[kernel][timer]: apic frequency %uhz\n", CPUS[0].lapic_timer.freq);

	x86_init_usermode();
	sched_init(true);
	kprintf("[kernel][smp]: sched init\n");

	for (usize i = 0; i < SMP_REQUEST.response->cpu_count; ++i) {
		struct limine_smp_info* info = SMP_REQUEST.response->cpus[i];
		if (info->lapic_id == SMP_REQUEST.response->bsp_lapic_id) {
			continue;
		}
		spinlock_lock(&START_LOCK);
		void* stack = vm_kernel_alloc_backed(2, PF_READ | PF_WRITE);
		spinlock_unlock(&START_LOCK);
		assert(stack);
		info->extra_argument = (u64) stack + 2 * PAGE_SIZE;
		info->goto_address = x86_ap_entry_asm;
	}

	while (atomic_load_explicit(&CPUS_ONLINE, memory_order_relaxed) != SMP_REQUEST.response->cpu_count) {
		arch_spinloop_hint();
	}

	lapic_timer_init_final();
	lapic_timer_start(100);

	kprintf("[kernel][x86]: smp init done\n");
}

usize arch_get_cpu_count() {
	return CPU_COUNT;
}

Cpu* arch_get_cpu(usize index) {
	return &CPUS[index].common;
}