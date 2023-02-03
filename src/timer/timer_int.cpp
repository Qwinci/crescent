#include "timer_int.hpp"
#include "acpi/lapic.hpp"
#include "console.hpp"
#include "sched/sched.hpp"
#include "utils/misc.hpp"

u16 timer_vec {};
static u64 timer_us = 0;
static usize current_us = 0;

extern SchedLevel levels[32];
extern Task* sleeping_tasks;
extern Task* current_task;

void start_timer() {
	current_us = US_IN_SEC;
	Lapic::start_oneshot(1, timer_vec);
}

template<typename T>
constexpr T max(T value1, T value2) {
	return value1 > value2 ? value1 : value2;
}

template<typename T>
constexpr T min(T value1, T value2) {
	return value1 < value2 ? value1 : value2;
}

void timer_int(InterruptCtx* ctx) {
	Lapic::eoi();
	timer_us += current_us;

	auto flags = enter_critical();

	bool all_empty = true;
	for (const auto& level : levels) {
		if (!level.is_empty()) {
			all_empty = false;
			break;
		}
	}

	auto& level = levels[current_task->task_level];
	u64 next_timestamp = level.slice_us;

	bool sleep = false;
	while (sleeping_tasks) {
		if (sleeping_tasks->sleep_end > timer_us) {
			next_timestamp = min(next_timestamp, sleeping_tasks->sleep_end - timer_us);
			break;
		}
		else {
			auto task = sleeping_tasks;
			sleeping_tasks = task->next;
			sched_unblock(task);
			sleep = true;
		}
	}

	if (current_task->status == TaskStatus::Running) {
		current_task->decrease_level();
	}

	if (all_empty && !sleep) {
		next_timestamp = US_IN_SEC;
	}

	current_us = next_timestamp;

	leave_critical(flags);

	Lapic::start_oneshot(US_IN_SEC / next_timestamp, timer_vec);
	sched();
}

u64 get_timer_us() {
	return timer_us;
}