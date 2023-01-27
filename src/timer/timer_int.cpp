#include "timer_int.hpp"
#include "acpi/lapic.hpp"
#include "console.hpp"
#include "sched/sched.hpp"

u16 timer_vec {};
static u64 timer_us = 0;
static usize current_us = 0;
constexpr usize US_IN_SEC = 1000 * 1000;

extern Task* sleeping_tasks;
extern Task* sleeping_tasks_end;

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

void timer_int(InterruptCtx*) {
	println("timer int");
	Lapic::eoi();
	timer_us += current_us;

	u64 next_timestamp = US_IN_SEC;

	current_us = US_IN_SEC;

	bool done = false;

	while (sleeping_tasks) {
		if (sleeping_tasks->sleep_end > timer_us) {
			next_timestamp = min(next_timestamp, max(sleeping_tasks->sleep_end - timer_us, US_IN_SEC));
			break;
		}
		else {
			auto task = sleeping_tasks;
			sleeping_tasks = sleeping_tasks->next;
			Lapic::start_oneshot(US_IN_SEC / next_timestamp, timer_vec);
			unblock_task(task);
			done = true;
			break;
			// todo multiple sleeping tasks (sched delay)
		}
	}
	if (!sleeping_tasks) {
		sleeping_tasks_end = nullptr;
	}

	if (!done) {
		Lapic::start_oneshot(US_IN_SEC / next_timestamp, timer_vec);
	}
}

u64 get_timer_us() {
	return timer_us;
}