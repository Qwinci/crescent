#include "timer_int.hpp"
#include "acpi/lapic.hpp"
#include "scheduling/scheduler.hpp"

u64 timer_ns_since_boot = 0;
extern Process* sleeping_tasks;;
static u64 current_period_ns;

void start_lapic_timer() {
	current_period_ns = 1000000000ULL / 2;
	Lapic::start_oneshot(current_period_ns / 1000, 0);
}

[[gnu::interrupt]] void timer_handler(InterruptFrame*) {
	lock_scheduler_misc();
	timer_ns_since_boot += current_period_ns;
	println("timer with period ", current_period_ns, "ns");
	current_period_ns = 0;

	auto task = sleeping_tasks;
	Process* prev = nullptr;
	while (task) {
		if (task->sleep_end <= timer_ns_since_boot) {
			if (prev) {
				prev->next = task->next;
			}
			unblock_task(task);
		}
		else {
			current_period_ns = task->sleep_end - timer_ns_since_boot;
			Lapic::start_oneshot(current_period_ns / 1000, 0);
			break;
		}
		prev = task;
		task = task->next;
	}

	if (current_period_ns == 0) {
		current_period_ns = 1000000000ULL / 2;
		Lapic::start_oneshot(current_period_ns / 1000, 0);
	}

	unlock_scheduler_misc();

	Lapic::eoi();
	scheduler_lock.lock();
	schedule();
	scheduler_lock.unlock();
}