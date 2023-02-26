#include "timer_int.hpp"
#include "arch.hpp"
#include "arch/x86/lapic.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "sched/sched.hpp"
#include "utils/math.hpp"
#include "utils/misc.hpp"

u16 timer_vec {};
static u64 timer_us = 0;
static usize current_us = 0;

void start_timer() {
	current_us = US_IN_SEC;
	Lapic::start_oneshot(1, timer_vec);
}

void timer_int(InterruptCtx* ctx, void*) {
	Lapic::eoi();
	timer_us += current_us;

	auto flags = enter_critical();

	auto local = arch_get_cpu_local();

	const auto& levels = local->levels;

	bool all_empty = true;
	for (const auto& level : levels) {
		if (!level.is_empty()) {
			all_empty = false;
			break;
		}
	}

	auto& level = levels[local->current_task->task_level];
	u64 next_timestamp = level.slice_us;

	while (auto task = local->blocked_tasks[as<u8>(TaskStatus::Sleeping)]) {
		if (task->sleep_end > timer_us) {
			next_timestamp = min(next_timestamp, task->sleep_end - timer_us);
			break;
		}
		else {
			local->blocked_tasks[as<u8>(TaskStatus::Sleeping)] = task->next;
			sched_unblock(task);
		}
	}

	if (local->current_task->status == TaskStatus::Running) {
		local->current_task->decrease_level();
	}

	if (all_empty) {
		next_timestamp = US_IN_SEC;
	}

	current_us = next_timestamp;

	leave_critical(flags);

	Lapic::start_oneshot(US_IN_SEC / next_timestamp, timer_vec);
	local->timer.trigger_timers(timer_us);
	sched();
}

u64 get_timer_us() {
	return timer_us;
}