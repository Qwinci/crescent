#include "lapic_timer.h"
#include "arch/interrupts.h"
#include "arch/x86/cpu.h"
#include "assert.h"
#include "lapic.h"
#include "mem/allocator.h"
#include "utils/math.h"
#include "sched/sched_internals.h"

#define LAPIC_TIMER_ONESHOT 0
#define LAPIC_TIMER_PERIODIC (1 << 17)
#define LAPIC_INT_MASKED 0x10000

static u8 timer_vec = 0;

usize lapic_timer_get_ns() {
	return x86_get_cur_cpu()->lapic_timer.us * NS_IN_US;
}

static IrqStatus lapic_timer_int(void*, void*) {
	X86Cpu* cpu = x86_get_cur_cpu();

	cpu->lapic_timer.us += cpu->lapic_timer.period_us;

	usize next_us = US_IN_SEC;
	bool do_sched = false;
	while (cpu->common.blocked_tasks[TASK_STATUS_SLEEPING]) {
		Task* task = cpu->common.blocked_tasks[TASK_STATUS_SLEEPING];
		if (task->sleep_end > cpu->lapic_timer.us) {
			next_us = MIN(next_us, task->sleep_end - cpu->lapic_timer.us);
			break;
		}
		else {
			cpu->common.blocked_tasks[TASK_STATUS_SLEEPING] = task->next;
			do_sched |= sched_unblock(task);
		}
	}

	Task* cur_task = cpu->common.current_task;

	if (cur_task->status == TASK_STATUS_RUNNING && cur_task->level && !cur_task->pin_level) {
		cpu->common.current_task->level -= 1;
	}

	usize us_since_switch = cpu->lapic_timer.us - cpu->lapic_timer.last_sched_us;

	if (us_since_switch >= cpu->common.sched_levels[cur_task->level].slice_us) {
		do_sched = true;
	}

	if (do_sched) {
		cpu->lapic_timer.last_sched_us = cpu->lapic_timer.us;
		Task* next = sched_get_next_task();

		if (next) {
			next_us = MIN(next_us, cpu->common.sched_levels[next->level].slice_us);
		}

		cpu->lapic_timer.period_us = next_us;
		lapic_timer_start(US_IN_SEC / next_us);
		sched_with_next(next);
	}
	else {
		cpu->lapic_timer.period_us = next_us;
		lapic_timer_start(US_IN_SEC / next_us);
	}

	return IRQ_ACK;
}

static IrqStatus tmp_handler_fn(void*, void*) {
	return IRQ_ACK;
}

void lapic_timer_start(usize freq) {
	X86Cpu* cpu = x86_get_cur_cpu();
	cpu->lapic_timer.period_us = 1000 * 1000 / freq;
	u64 value = cpu->lapic_timer.freq / 16 / freq;
	if (value == 0) {
		panic("lapic oneshot frequency is too high");
	}
	lapic_write(LAPIC_REG_INIT_COUNT, value);
	lapic_write(LAPIC_REG_LVT_TIMER, timer_vec | LAPIC_TIMER_ONESHOT);
}

static IrqHandler LAPIC_TIMER_HANDLER = {
	.fn = lapic_timer_int,
	.userdata = NULL
};

static void lapic_timer_calibrate(X86Cpu* cpu) {
	// 16
	lapic_write(LAPIC_REG_DIV_CONF, 3);

	IrqHandler tmp_handler = {
		.fn = tmp_handler_fn,
		.userdata = NULL
	};

	if (!timer_vec) {
		timer_vec = arch_irq_alloc_generic(IPL_TIMER, 1, IRQ_INSTALL_FLAG_NONE);
		assert(timer_vec && "failed to allocate timer interrupt");
		assert(arch_irq_install(timer_vec, &tmp_handler, IRQ_INSTALL_FLAG_NONE) == IRQ_INSTALL_STATUS_SUCCESS);
	}

	lapic_write(LAPIC_REG_LVT_TIMER, timer_vec | LAPIC_TIMER_ONESHOT);
	lapic_write(LAPIC_REG_INIT_COUNT, 0xFFFFFFFF);

	udelay(10 * US_IN_MS);

	lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_INT_MASKED);
	usize ticks_in_1s = (0xFFFFFFFF - lapic_read(LAPIC_REG_CURR_COUNT)) * 16 * 100;

	cpu->lapic_timer.freq = ticks_in_1s;

	lapic_write(LAPIC_REG_DIV_CONF, 3);

	assert(arch_irq_remove(timer_vec, &tmp_handler) == IRQ_REMOVE_STATUS_SUCCESS);
	assert(arch_irq_install(timer_vec, &LAPIC_TIMER_HANDLER, IRQ_INSTALL_FLAG_NONE) == IRQ_INSTALL_STATUS_SUCCESS);
}

void lapic_timer_init() {
	X86Cpu* cpu = x86_get_cur_cpu();

	lapic_timer_calibrate(cpu);
}
