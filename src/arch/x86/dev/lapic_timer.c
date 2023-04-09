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

static bool lapic_timer_int(void*, void*) {
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
			do_sched = sched_unblock(task);
		}
	}

	Task* cur_task = cpu->common.current_task;

	if (cur_task->status == TASK_STATUS_RUNNING && cur_task->level && !cur_task->pin_level) {
		cpu->common.current_task->level -= 1;
	}

	cpu->lapic_timer.period_us = next_us;
	lapic_timer_start(US_IN_SEC / next_us);

	if (cpu->lapic_timer.us >= cpu->lapic_timer.last_sched_us + cpu->common.sched_levels[cur_task->level].slice_us || do_sched) {
		cpu->lapic_timer.last_sched_us = cpu->lapic_timer.us;
		Task* next = sched_get_next_task();
		if (next) {
			next_us = MIN(next_us, cpu->common.sched_levels[next->level].slice_us);
		}
		sched_with_next(next);
	}

	return true;
}

static bool tmp_handler(void*, void*) {
	return true;
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

static void lapic_timer_calibrate(X86Cpu* cpu) {
	// 16
	lapic_write(LAPIC_REG_DIV_CONF, 3);

	if (!timer_vec) {
		timer_vec = arch_alloc_int(tmp_handler, NULL);
		assert(timer_vec && "failed to allocate timer interrupt");
	}

	lapic_write(LAPIC_REG_LVT_TIMER, timer_vec | LAPIC_TIMER_ONESHOT);
	lapic_write(LAPIC_REG_INIT_COUNT, 0xFFFFFFFF);

	udelay(10 * 1000);

	lapic_write(LAPIC_REG_LVT_TIMER, LAPIC_INT_MASKED);
	usize ticks_in_1s = (0xFFFFFFFF - lapic_read(LAPIC_REG_CURR_COUNT)) * 16 * 100;

	cpu->lapic_timer.freq = ticks_in_1s;

	lapic_write(LAPIC_REG_DIV_CONF, 3);

	arch_set_handler(timer_vec, lapic_timer_int, NULL);
}

void lapic_timer_init() {
	X86Cpu* cpu = x86_get_cur_cpu();

	lapic_timer_calibrate(cpu);
}
